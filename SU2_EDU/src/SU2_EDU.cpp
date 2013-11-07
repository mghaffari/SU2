/*!
 * \file SU2_CFD.cpp
 * \brief Main file of Computational Fluid Dynamics Code (SU2_CFD).
 * \author Aerospace Design Laboratory (Stanford University) <http://su2.stanford.edu>.
 * \version 2.0.8
 *
 * Stanford University Unstructured (SU2).
 * Copyright (C) 2012-2013 Aerospace Design Laboratory (ADL).
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/SU2_EDU.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  bool StopCalc = false;
  unsigned long StartTime, StopTime, TimeUsed = 0, ExtIter = 0;
  unsigned short iMesh, iZone, iSol, nZone, nDim;
  ofstream ConvHist_file;
  int rank = MASTER_NODE;
  
#ifndef NO_MPI
  /*--- MPI initialization, and buffer setting ---*/
  void *buffer, *old_buffer;
  int size, bufsize;
  bufsize = MAX_MPI_BUFFER;
  buffer = new char[bufsize];
  MPI::Init(argc, argv);
  MPI::Attach_buffer(buffer, bufsize);
  rank = MPI::COMM_WORLD.Get_rank();
  size = MPI::COMM_WORLD.Get_size();
#ifdef TIME
  /*--- Set up a timer for parallel performance benchmarking ---*/
  double start, finish, time;
  MPI::COMM_WORLD.Barrier();
  start = MPI::Wtime();
#endif
#endif
  
  /*--- Create pointers to all of the classes that may be used throughout
   the SU2_CFD code. In general, the pointers are instantiated down a
   heirarchy over all zones, multigrid levels, equation sets, and equation
   terms as described in the comments below. ---*/
  
  COutput *output                       = NULL;
  CIntegration ***integration_container = NULL;
  CGeometry ***geometry_container       = NULL;
  CSolver ****solver_container          = NULL;
  CNumerics *****numerics_container     = NULL;
  CConfig **config_container            = NULL;
  CSurfaceMovement **surface_movement   = NULL;
  CVolumetricMovement **grid_movement   = NULL;
  CFreeFormDefBox*** FFDBox             = NULL;
  
  /*--- Load in the number of zones and spatial dimensions in the mesh file (If no config
   file is specified, default.cfg is used) ---*/
  
  char config_file_name[200];
  if (argc == 2){ strcpy(config_file_name,argv[1]); }
  else{ strcpy(config_file_name, "default.cfg"); }
  
  /*--- Read the name and format of the input mesh file ---*/
  
  CConfig *config = NULL;
  config = new CConfig(config_file_name);
  
  /*--- Get the number of zones and dimensions from the numerical grid
   (required for variables allocation) ---*/
  
  nZone = GetnZone(config->GetMesh_FileName(), config->GetMesh_FileFormat(), config);
  nDim  = GetnDim(config->GetMesh_FileName(), config->GetMesh_FileFormat());
  
  /*--- Definition and of the containers for all possible zones. ---*/
  
  solver_container      = new CSolver***[nZone];
  integration_container = new CIntegration**[nZone];
  numerics_container    = new CNumerics****[nZone];
  config_container      = new CConfig*[nZone];
  geometry_container    = new CGeometry **[nZone];
  surface_movement      = new CSurfaceMovement *[nZone];
  grid_movement         = new CVolumetricMovement *[nZone];
  FFDBox                = new CFreeFormDefBox**[nZone];
  
  for (iZone = 0; iZone < nZone; iZone++) {
    solver_container[iZone]       = NULL;
    integration_container[iZone]  = NULL;
    numerics_container[iZone]     = NULL;
    config_container[iZone]       = NULL;
    geometry_container[iZone]     = NULL;
    surface_movement[iZone]       = NULL;
    grid_movement[iZone]          = NULL;
    FFDBox[iZone]                 = NULL;
  }
  
  /*--- Loop over all zones to initialize the various classes. In most
   cases, nZone is equal to one. This represents the solution of a partial
   differential equation on a single block, unstructured mesh. ---*/
  
  for (iZone = 0; iZone < nZone; iZone++) {
    
    /*--- Definition of the configuration option class for all zones. In this
     constructor, the input configuration file is parsed and all options are
     read and stored. ---*/
    
    config_container[iZone] = new CConfig(config_file_name, SU2_CFD, iZone, nZone, VERB_HIGH);
    
#ifndef NO_MPI
    /*--- Change the name of the input-output files for a parallel computation ---*/
    config_container[iZone]->SetFileNameDomain(rank+1);
#endif
    
    /*--- Perform the non-dimensionalization for the flow equations using the
     specified reference values. ---*/
    
    config_container[iZone]->SetNondimensionalization(nDim, iZone);
    
    /*--- Definition of the geometry class. Within this constructor, the
     mesh file is read and the primal grid is stored (node coords, connectivity,
     & boundary markers. MESH_0 is the index of the finest mesh. ---*/
    
    geometry_container[iZone] = new CGeometry *[config_container[iZone]->GetMGLevels()+1];
    geometry_container[iZone][MESH_0] = new CPhysicalGeometry(config_container[iZone],
                                                              iZone+1, nZone);
    
  }
  
  if (rank == MASTER_NODE)
    cout << endl <<"------------------------- Geometry Preprocessing ------------------------" << endl;
  
  /*--- Preprocessing of the geometry for all zones. In this routine, the edge-
   based data structure is constructed, i.e. node and cell neighbors are
   identified and linked, face areas and volumes of the dual mesh cells are
   computed, and the multigrid levels are created using an agglomeration procedure. ---*/
  
  Geometrical_Preprocessing(geometry_container, config_container, nZone);
  
#ifndef NO_MPI
  /*--- Synchronization point after the geometrical definition subroutine ---*/
  MPI::COMM_WORLD.Barrier();
#endif
  
  if (rank == MASTER_NODE)
    cout << endl <<"------------------------- Solver Preprocessing --------------------------" << endl;
  
  for (iZone = 0; iZone < nZone; iZone++) {
    
    /*--- Definition of the solver class: solver_container[#ZONES][#MG_GRIDS][#EQ_SYSTEMS].
     The solver classes are specific to a particular set of governing equations,
     and they contain the subroutines with instructions for computing each spatial
     term of the PDE, i.e. loops over the edges to compute convective and viscous
     fluxes, loops over the nodes to compute source terms, and routines for
     imposing various boundary condition type for the PDE. ---*/
    
    solver_container[iZone] = new CSolver** [config_container[iZone]->GetMGLevels()+1];
    for (iMesh = 0; iMesh <= config_container[iZone]->GetMGLevels(); iMesh++)
      solver_container[iZone][iMesh] = NULL;
    
    for (iMesh = 0; iMesh <= config_container[iZone]->GetMGLevels(); iMesh++) {
      solver_container[iZone][iMesh] = new CSolver* [MAX_SOLS];
      for (iSol = 0; iSol < MAX_SOLS; iSol++)
        solver_container[iZone][iMesh][iSol] = NULL;
    }
    Solver_Preprocessing(solver_container[iZone], geometry_container[iZone],
                         config_container[iZone], iZone);
    
#ifndef NO_MPI
    /*--- Synchronization point after the solution preprocessing subroutine ---*/
    MPI::COMM_WORLD.Barrier();
#endif
    
    if (rank == MASTER_NODE)
      cout << endl <<"----------------- Integration and Numerics Preprocessing ----------------" << endl;
    
    /*--- Definition of the integration class: integration_container[#ZONES][#EQ_SYSTEMS].
     The integration class orchestrates the execution of the spatial integration
     subroutines contained in the solver class (including multigrid) for computing
     the residual at each node, R(U) and then integrates the equations to a
     steady state or time-accurately. ---*/
    
    integration_container[iZone] = new CIntegration*[MAX_SOLS];
    Integration_Preprocessing(integration_container[iZone], geometry_container[iZone],
                              config_container[iZone], iZone);
    
#ifndef NO_MPI
    /*--- Synchronization point after the integration definition subroutine ---*/
    MPI::COMM_WORLD.Barrier();
#endif
    
    /*--- Definition of the numerical method class:
     numerics_container[#ZONES][#MG_GRIDS][#EQ_SYSTEMS][#EQ_TERMS].
     The numerics class contains the implementation of the numerical methods for
     evaluating convective or viscous fluxes between any two nodes in the edge-based
     data structure (centered, upwind, galerkin), as well as any source terms
     (piecewise constant reconstruction) evaluated in each dual mesh volume. ---*/
    
    numerics_container[iZone] = new CNumerics***[config_container[iZone]->GetMGLevels()+1];
    Numerics_Preprocessing(numerics_container[iZone], solver_container[iZone],
                           geometry_container[iZone], config_container[iZone], iZone);
    
#ifndef NO_MPI
    /*--- Synchronization point after the solver definition subroutine ---*/
    MPI::COMM_WORLD.Barrier();
#endif
    
    /*--- Computation of wall distances for turbulence modeling ---*/
    
    if ( (config_container[iZone]->GetKind_Solver() == RANS)     ||
        (config_container[iZone]->GetKind_Solver() == ADJ_RANS)    )
      geometry_container[iZone][MESH_0]->ComputeWall_Distance(config_container[iZone]);
    
    /*--- Computation of positive surface area in the z-plane which is used for
     the calculation of force coefficient (non-dimensionalization). ---*/
    
    geometry_container[iZone][MESH_0]->SetPositive_ZArea(config_container[iZone]);
    
    /*--- Set the near-field and interface boundary conditions, if necessary. ---*/
    
    for (iMesh = 0; iMesh <= config_container[iZone]->GetMGLevels(); iMesh++) {
      geometry_container[iZone][iMesh]->MatchNearField(config_container[iZone]);
      geometry_container[iZone][iMesh]->MatchInterface(config_container[iZone]);
    }
    
  }
  
  /*--- Definition of the output class (one for all zones). The output class
   manages the writing of all restart, volume solution, surface solution,
   surface comma-separated value, and convergence history files (both in serial
   and in parallel). ---*/
  
  output = new COutput();
  
  /*--- Open the convergence history file ---*/
  
  if (rank == MASTER_NODE)
    output->SetHistory_Header(&ConvHist_file, config_container[ZONE_0]);
  
  /*--- Check for an unsteady restart. Update ExtIter if necessary. ---*/
  if (config_container[ZONE_0]->GetWrt_Unsteady() && config_container[ZONE_0]->GetRestart())
    ExtIter = config_container[ZONE_0]->GetUnst_RestartIter();
  
  /*--- Main external loop of the solver. Within this loop, each iteration ---*/
  
  if (rank == MASTER_NODE)
    cout << endl <<"------------------------------ Begin Solver -----------------------------" << endl;
  
  while (ExtIter < config_container[ZONE_0]->GetnExtIter()) {
    
    /*--- Set a timer for each iteration. Store the current iteration and
     update  the value of the CFL number (if there is CFL ramping specified)
     in the config class. ---*/
    
    StartTime = clock();
    for (iZone = 0; iZone < nZone; iZone++) {
      config_container[iZone]->SetExtIter(ExtIter);
      config_container[iZone]->UpdateCFL(ExtIter);
    }
    
    /*--- Perform a single iteration of the chosen PDE solver. ---*/
    MeanFlowIteration(output, integration_container, geometry_container,
                      solver_container, numerics_container, config_container,
                      surface_movement, grid_movement, FFDBox);
    
    /*--- Synchronization point after a single solver iteration. Compute the
     wall clock time required. ---*/
    
#ifndef NO_MPI
    MPI::COMM_WORLD.Barrier();
#endif
    StopTime = clock(); TimeUsed += (StopTime - StartTime);
    
    /*--- Update the convergence history file (serial and parallel computations). ---*/
    
    output->SetConvergence_History(&ConvHist_file, geometry_container, solver_container,
                                   config_container, integration_container, false, TimeUsed, ZONE_0);
    
    /*--- Check whether the current simulation has reached the specified
     convergence criteria, and set StopCalc to true, if so. ---*/
    
    switch (config_container[ZONE_0]->GetKind_Solver()) {
      case EULER: case NAVIER_STOKES: case RANS:
        StopCalc = integration_container[ZONE_0][FLOW_SOL]->GetConvergence(); break;
    }
    
    /*--- Solution output. Determine whether a solution needs to be written
     after the current iteration, and if so, execute the output file writing
     routines. ---*/
    
    if ((ExtIter+1 == config_container[ZONE_0]->GetnExtIter()) ||
        ((ExtIter % config_container[ZONE_0]->GetWrt_Sol_Freq() == 0) && (ExtIter != 0) &&
         !((config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
           (config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_2ND))) ||
        (StopCalc) ||
        (((config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
          (config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_2ND)) &&
         ((ExtIter == 0) || (ExtIter % config_container[ZONE_0]->GetWrt_Sol_Freq_DualTime() == 0)))) {
          
          /*--- Execute the routine for writing restart, volume solution,
           surface solution, and surface comma-separated value files. ---*/
          
          output->SetResult_Files(solver_container, geometry_container, config_container, ExtIter, nZone);
          
        }
    
    /*--- If the convergence criteria has been met, terminate the simulation. ---*/
    
    if (StopCalc) break;
    ExtIter++;
    
  }
  
  /*--- Close the convergence history file. ---*/
  
  if (rank == MASTER_NODE) {
    ConvHist_file.close();
    cout << endl <<"History file, closed." << endl;
  }
  
  /*--- Solver class deallocation ---*/
  //  for (iZone = 0; iZone < nZone; iZone++) {
  //    for (iMesh = 0; iMesh <= config_container[iZone]->GetMGLevels(); iMesh++) {
  //      for (iSol = 0; iSol < MAX_SOLS; iSol++) {
  //        if (solver_container[iZone][iMesh][iSol] != NULL) {
  //          delete solver_container[iZone][iMesh][iSol];
  //        }
  //      }
  //      delete solver_container[iZone][iMesh];
  //    }
  //    delete solver_container[iZone];
  //  }
  //  delete [] solver_container;
  //  if (rank == MASTER_NODE) cout <<"Solution container, deallocated." << endl;
  
  /*--- Geometry class deallocation ---*/
  //  for (iZone = 0; iZone < nZone; iZone++) {
  //    for (iMesh = 0; iMesh <= config_container[iZone]->GetMGLevels(); iMesh++) {
  //      delete geometry_container[iZone][iMesh];
  //    }
  //    delete geometry_container[iZone];
  //  }
  //  delete [] geometry_container;
  //  cout <<"Geometry container, deallocated." << endl;
  
  /*--- Integration class deallocation ---*/
  //  cout <<"Integration container, deallocated." << endl;
  
#ifndef NO_MPI
  /*--- Compute/print the total time for parallel performance benchmarking. ---*/
#ifdef TIME
  MPI::COMM_WORLD.Barrier();
  finish = MPI::Wtime();
  time = finish-start;
  if (rank == MASTER_NODE) {
    cout << "\nCompleted in " << fixed << time << " seconds on "<< size;
    if (size == 1) cout << " core.\n" << endl;
    else cout << " cores.\n" << endl;
  }
#endif
  /*--- Finalize MPI parallelization ---*/
  old_buffer = buffer;
  MPI::Detach_buffer(old_buffer);
  //	delete [] buffer;
  MPI::Finalize();
#endif
  
  /*--- Exit the solver cleanly ---*/
  
  if (rank == MASTER_NODE)
    cout << endl <<"------------------------- Exit Success (SU2_CFD) ------------------------" << endl << endl;
  
  return EXIT_SUCCESS;
}