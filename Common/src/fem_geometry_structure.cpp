/*!
 * \file fem_geometry_structure.cpp
 * \brief Main subroutines for creating the primal grid for the FEM solver.
 * \author E. van der Weide
 * \version 4.1.0 "Cardinal"
 *
 * SU2 Lead Developers: Dr. Francisco Palacios (Francisco.D.Palacios@boeing.com).
 *                      Dr. Thomas D. Economon (economon@stanford.edu).
 *
 * SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
 *                 Prof. Piero Colonna's group at Delft University of Technology.
 *                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *                 Prof. Rafael Palacios' group at Imperial College London.
 *
 * Copyright (C) 2012-2015 SU2, the open-source CFD code.
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

#include "../include/fem_geometry_structure.hpp"

/* MKL, BLAS and LAPACK include files, if supported. */
#ifdef HAVE_MKL
#include "mkl.h"
#else

#ifdef HAVE_LAPACK
#include "lapacke.h"
#endif
#ifdef HAVE_CBLAS
#include "cblas.h"
#endif

#endif

bool SortFacesClass::operator()(const FaceOfElementClass &f0,
                                const FaceOfElementClass &f1) {

  /*--- Comparison in case both faces are boundary faces. ---*/
  if(f0.faceIndicator >= 0 && f1.faceIndicator >= 0) {

    /* Both faces are boundary faces. The first comparison is the boundary
       marker, which is stored in faceIndicator. */
    if(f0.faceIndicator != f1.faceIndicator) return f0.faceIndicator < f1.faceIndicator;

    /* Both faces belong to the same boundary marker. Make sure that the
       sequence of the faces is identical to the sequence stored in the
       surface connectivity of the boundary. This information is stored in
       either nPolyGrid0 or nPolyGrid1, depending on which side of the face
       the corresponding element is located. */
    unsigned long ind0 = f0.elemID0 < nVolElemTot ? f0.nPolyGrid1 : f0.nPolyGrid0;
    unsigned long ind1 = f1.elemID0 < nVolElemTot ? f1.nPolyGrid1 : f1.nPolyGrid0;

    return ind0 < ind1;
  }

  /*--- Comparison in case both faces are internal faces. ---*/
  if(f0.faceIndicator == -1 && f1.faceIndicator == -1) {

    /* Both faces are internal faces. First determine the minimum and maximum
       ID of its adjacent elements.  */
    unsigned long elemIDMin0 = min(f0.elemID0, f0.elemID1);
    unsigned long elemIDMax0 = max(f0.elemID0, f0.elemID1);

    unsigned long elemIDMin1 = min(f1.elemID0, f1.elemID1);
    unsigned long elemIDMax1 = max(f1.elemID0, f1.elemID1);

    /* Determine the situation. */
    if(elemIDMax0 < nVolElemTot && elemIDMax1 < nVolElemTot) {

      /* Both faces are matching internal faces. These faces are sorted
         according to their element ID's in order to increase cache performance.
         This may be adapted to improve performance. */
      if(elemIDMin0 != elemIDMin1) return elemIDMin0 < elemIDMin1;
      return elemIDMax0 < elemIDMax1;
    }
    else if(elemIDMax0 >= nVolElemTot && elemIDMax1 >= nVolElemTot) {

      /* Both faces are non-matching internal faces. Sort them according to
         their relevant element ID. */
      return elemIDMin0 < elemIDMin1;
    }
    else {

      /* One face is a matching internal face and the other face is a
         non-matching internal face. Make sure that the non-matching face
         is numbered after the matching face. This is accomplished by comparing
         the maximum element ID's. */
      return elemIDMax0 < elemIDMax1;
    }
  }

  /*--- One face is a boundary face and the other face is an internal face. ---*/
  /*--- Make sure that the boundary face is numbered first. This can be     ---*/
  /*--- accomplished by using the > operator for faceIndicator.             ---*/
  return f0.faceIndicator > f1.faceIndicator;
}

void CPointCompare::Copy(const CPointCompare &other) {
  nDim           = other.nDim;
  nodeID         = other.nodeID;
  tolForMatching = other.tolForMatching;

  for(unsigned short l=0; l<nDim; ++l)
    coor[l] = other.coor[l];
}

bool CPointCompare::operator< (const CPointCompare &other) const {
  if(nDim != other.nDim) return nDim < other.nDim;    // This should never be active.

  su2double tol = min(tolForMatching, other.tolForMatching); // Tolerance for comparing.
  for(unsigned short l=0; l<nDim; ++l) {
    if(fabs(coor[l] - other.coor[l]) > tol)
      return coor[l] < other.coor[l];
  }

  return false;  // Both objects are identical.
}

void CPointFEM::Copy(const CPointFEM &other) {
  globalID           = other.globalID;
  periodIndexToDonor = other.periodIndexToDonor;
  coor[0]            = other.coor[0];
  coor[1]            = other.coor[1];
  coor[2]            = other.coor[2];
}

bool CPointFEM::operator< (const CPointFEM &other) const {
  if(periodIndexToDonor != other.periodIndexToDonor)
    return periodIndexToDonor < other.periodIndexToDonor;
  return globalID < other.globalID;
 }

bool CPointFEM::operator==(const CPointFEM &other) const {
 return (globalID           == other.globalID &&
         periodIndexToDonor == other.periodIndexToDonor);
}

void CVolumeElementFEM::GetCornerPointsAllFaces(unsigned short &numFaces,
                                                unsigned short nPointsPerFace[],
                                                unsigned long  faceConn[6][4]) {

  /*--- Get the corner connectivities of the faces, local to the element. ---*/
  CPrimalGridFEM::GetLocalCornerPointsAllFaces(VTK_Type, nPolyGrid, nDOFsGrid,
                                               numFaces, nPointsPerFace, faceConn);

  /*--- Convert the local values of faceConn to global values. ---*/
  for(unsigned short i=0; i<numFaces; ++i) {
    for(unsigned short j=0; j<nPointsPerFace[i]; ++j) {
      unsigned long nn = faceConn[i][j];
      faceConn[i][j] = nodeIDsGrid[nn];
    }
  }
}

void CSurfaceElementFEM::GetCornerPointsFace(unsigned short &nPointsPerFace,
                                             unsigned long  faceConn[]) {

  /*--- Get the corner connectivities of the face, local to the element. ---*/
  CPrimalGridBoundFEM::GetLocalCornerPointsFace(VTK_Type, nPolyGrid, nDOFsGrid,
                                                nPointsPerFace, faceConn);

  /*--- Convert the local values of faceConn to global values. ---*/
  for(unsigned short j=0; j<nPointsPerFace; ++j) {
    unsigned long nn = faceConn[j];
    faceConn[j] = nodeIDsGrid[nn];
  }
}

su2double CSurfaceElementFEM::DetermineLengthScale(vector<CPointFEM> &meshPoints) {

  /* Variables needed to make a generic treatment possible. */
  unsigned short nDim;
  unsigned short nEdges;
  unsigned long  edgeVertices[4][2];

  su2double lenScale;

  /*--- A distinction must be made between element types. As this is a surface
        element the only options are a line, a triangle and a quadrilateral.
        Determine the number of edges and its connectivities.             ---*/
  switch( VTK_Type ) {

    case LINE: {
      nEdges = 1; nDim = 2;
      edgeVertices[0][0] = nodeIDsGrid.front();
      edgeVertices[0][1] = nodeIDsGrid.back();
      break;
    }

    case TRIANGLE: {
      nEdges = 3; nDim = 3;
      edgeVertices[0][0] = nodeIDsGrid.front();
      edgeVertices[0][1] = nodeIDsGrid[nPolyGrid];

      edgeVertices[1][0] = nodeIDsGrid[nPolyGrid];
      edgeVertices[1][1] = nodeIDsGrid.back();

      edgeVertices[2][0] = nodeIDsGrid.back();
      edgeVertices[2][1] = nodeIDsGrid.front();
      break;
    }

    case QUADRILATERAL: {
      nEdges = 4; nDim = 3;
      edgeVertices[0][0] = nodeIDsGrid.front();
      edgeVertices[0][1] = nodeIDsGrid[nPolyGrid];

      edgeVertices[1][0] = nodeIDsGrid[nPolyGrid];
      edgeVertices[1][1] = nodeIDsGrid.back();

      edgeVertices[2][0] = nodeIDsGrid.back();
      edgeVertices[2][1] = nodeIDsGrid[nPolyGrid*(nPolyGrid+1)];

      edgeVertices[3][0] = nodeIDsGrid[nPolyGrid*(nPolyGrid+1)];
      edgeVertices[3][1] = nodeIDsGrid.front();
      break;
    }

    default: {
      cout << "CSurfaceElementFEM::DetermineLengthScale: This should not happen." << endl;
#ifndef HAVE_MPI
      exit(EXIT_FAILURE);
#else
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
#endif
    }
  }

  /* Loop over the edges, determine their length and take the minimum
     for the length scale. */
  for(unsigned short i=0; i<nEdges; ++i) {
    unsigned long n0 = edgeVertices[i][0];
    unsigned long n1 = edgeVertices[i][1];

    su2double len = 0.0;
    for(unsigned short l=0; l<nDim; ++l) {
      su2double ds = meshPoints[n1].coor[l] - meshPoints[n0].coor[l];
      len += ds*ds;
    }
    len = sqrt(len);

    if(i == 0) lenScale = len;
    else       lenScale = min(lenScale, len);
  }

  /* Return the length scale. */
  return lenScale;
}

void CSurfaceElementFEM::Copy(const CSurfaceElementFEM &other) {
  VTK_Type           = other.VTK_Type;
  nPolyGrid          = other.nPolyGrid;
  nDOFsGrid          = other.nDOFsGrid;
  indStandardElement = other.indStandardElement;
  volElemID          = other.volElemID;
  boundElemIDGlobal  = other.boundElemIDGlobal;
  nodeIDsGrid        = other.nodeIDsGrid;
}

CMeshFEM::CMeshFEM(CGeometry *geometry, CConfig *config) {

  /*--- Determine the number of ranks and the current rank. ---*/
  int nRank = SINGLE_NODE;
  int rank  = MASTER_NODE;

#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nRank);
#endif

  /*--- Copy the number of dimensions. ---*/
  nDim = geometry->GetnDim();

  /*--- Determine a mapping from the global point ID to the local index
        of the points.            ---*/
  map<unsigned long,unsigned long> globalPointIDToLocalInd;
  for(unsigned i=0; i<geometry->GetnPoint(); ++i)
    globalPointIDToLocalInd[geometry->node[i]->GetGlobalIndex()] = i;

  /*----------------------------------------------------------------------------*/
  /*--- Step 1: Communicate the elements and the boundary elements to the    ---*/
  /*---         ranks where they will stored during the computation.         ---*/
  /*----------------------------------------------------------------------------*/

  /*--- Determine the ranks to which I have to send my elements. ---*/
  vector<int> sendToRank(nRank, 0);

  for(unsigned long i=0; i<geometry->GetnElem(); i++) {
    sendToRank[geometry->elem[i]->GetColor()] = 1;
  }

  map<int,int> rankToIndCommBuf;
  for(int i=0; i<nRank; ++i) {
    if( sendToRank[i] ) {
      int ind = rankToIndCommBuf.size();
      rankToIndCommBuf[i] = ind;
    }
  }

  /*--- Definition of the communication buffers, used to send the element data
        to the correct ranks.                ---*/
  int nRankSend = rankToIndCommBuf.size();
  vector<vector<short> >     shortSendBuf(nRankSend,  vector<short>(0));
  vector<vector<long>  >     longSendBuf(nRankSend,   vector<long>(0));
  vector<vector<su2double> > doubleSendBuf(nRankSend, vector<su2double>(0));

  /*--- The first element of longSendBuf will contain the number of elements, which
        are stored in the communication buffers. Initialize this value to 0. ---*/
  for(int i=0; i<nRankSend; ++i) longSendBuf[i].push_back(0);

  /*--- Determine the number of ranks, from which this rank will receive elements. ---*/
  int nRankRecv = nRankSend;

#ifdef HAVE_MPI
  vector<int> sizeRecv(nRank, 1);

  MPI_Reduce_scatter(sendToRank.data(), &nRankRecv, sizeRecv.data(),
                     MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif

  /*--- Loop over the local elements to fill the communication buffers with element data. ---*/
  for(unsigned long i=0; i<geometry->GetnElem(); ++i) {
    int ind = geometry->elem[i]->GetColor();
    map<int,int>::const_iterator MI = rankToIndCommBuf.find(ind);
    ind = MI->second;

    ++longSendBuf[ind][0];   /* The number of elements in the buffers must be incremented. */

    shortSendBuf[ind].push_back(geometry->elem[i]->GetVTK_Type());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNPolyGrid());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNPolySol());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNDOFsGrid());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetNDOFsSol());
    shortSendBuf[ind].push_back(geometry->elem[i]->GetnFaces());
    shortSendBuf[ind].push_back( (short) geometry->elem[i]->GetJacobianConsideredConstant());

    longSendBuf[ind].push_back(geometry->elem[i]->GetGlobalElemID());
    longSendBuf[ind].push_back(geometry->elem[i]->GetGlobalOffsetDOFsSol());

    for(unsigned short j=0; j<geometry->elem[i]->GetNDOFsGrid(); ++j)
      longSendBuf[ind].push_back(geometry->elem[i]->GetNode(j));

    for(unsigned short j=0; j<geometry->elem[i]->GetnFaces(); ++j)
      longSendBuf[ind].push_back(geometry->elem[i]->GetNeighbor_Elements(j));

    for(unsigned short j=0; j<geometry->elem[i]->GetnFaces(); ++j) {
      shortSendBuf[ind].push_back(geometry->elem[i]->GetPeriodicIndex(j));
      shortSendBuf[ind].push_back( (short) geometry->elem[i]->GetJacobianConstantFace(j));
    }
  }

  /*--- Determine for each rank to which I have to send elements the data of
        the corresponding nodes.   ---*/
  for(int i=0; i<nRankSend; ++i) {

    /*--- Determine the vector with node IDs in the connectivity
          of the elements for this rank.   ---*/
    vector<long> nodeIDs;

    unsigned long indL = 3;
    unsigned long indS = 3;
    for(long j=0; j<longSendBuf[i][0]; ++j) {
      short nDOFsGrid = shortSendBuf[i][indS], nFaces = shortSendBuf[i][indS+2];
      indS += 2*nFaces+7;

      for(short k=0; k<nDOFsGrid; ++k, ++indL)
        nodeIDs.push_back(longSendBuf[i][indL]);
      indL += nFaces+2;
    }

    /*--- Sort nodeIDs in increasing order and remove the double entities. ---*/
    sort(nodeIDs.begin(), nodeIDs.end());
    vector<long>::iterator lastNodeID = unique(nodeIDs.begin(), nodeIDs.end());
    nodeIDs.erase(lastNodeID, nodeIDs.end());

    /*--- Add the number of node IDs and the node IDs itself to longSendBuf[i]. ---*/
    longSendBuf[i].push_back(nodeIDs.size());
    longSendBuf[i].insert(longSendBuf[i].end(), nodeIDs.begin(), nodeIDs.end());

    /*--- Copy the coordinates to doubleSendBuf. ---*/
    for(unsigned long j=0; j<nodeIDs.size(); ++j) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalPointIDToLocalInd.find(nodeIDs[j]);

      if(LMI == globalPointIDToLocalInd.end()) {
        cout << "Entry not found in map in function CMeshFEM::CMeshFEM" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }

      unsigned long ind = LMI->second;
      for(unsigned short l=0; l<nDim; ++l)
        doubleSendBuf[i].push_back(geometry->node[ind]->GetCoord(l));
    }
  }

  /*--- Loop over the boundaries to send the boundary data to the appropriate rank. ---*/
  nMarker = geometry->GetnMarker();
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {

    /* Store the current indices in the longSendBuf, which are used to store the
       number of boundary elements sent to this rank. Initialize this value to 0. */
    vector<long> indLongBuf(nRankSend);
    for(int i=0; i<nRankSend; ++i) {
      indLongBuf[i] = longSendBuf[i].size();
      longSendBuf[i].push_back(0);
    }

    /* Loop over the local boundary elements in geometry for this marker. */
    for(unsigned long i=0; i<geometry->GetnElem_Bound(iMarker); ++i) {

      /* Determine the local ID of the corresponding domain element. */
      unsigned long elemID = geometry->bound[iMarker][i]->GetDomainElement()
                           - geometry->starting_node[rank];

      /* Determine to which rank this boundary element must be sent.
         That is the same as its corresponding domain element.
         Update the corresponding index in longSendBuf. */
      int ind = geometry->elem[elemID]->GetColor();
      map<int,int>::const_iterator MI = rankToIndCommBuf.find(ind);
      ind = MI->second;

      ++longSendBuf[ind][indLongBuf[ind]];

      /* Store the data for this boundary element in the communication buffers. */
      shortSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetVTK_Type());
      shortSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetNPolyGrid());
      shortSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetNDOFsGrid());

      longSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetDomainElement());
      longSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetGlobalElemID());

      for(unsigned short j=0; j<geometry->bound[iMarker][i]->GetNDOFsGrid(); ++j)
        longSendBuf[ind].push_back(geometry->bound[iMarker][i]->GetNode(j));
    }
  }

  /*--- Definition of the communication buffers, used to receive
        the element data from the other correct ranks.        ---*/
  vector<vector<short> >     shortRecvBuf(nRankRecv,  vector<short>(0));
  vector<vector<long>  >     longRecvBuf(nRankRecv,   vector<long>(0));
  vector<vector<su2double> > doubleRecvBuf(nRankRecv, vector<su2double>(0));

  /*--- Communicate the data to the correct ranks. Make a distinction
        between parallel and sequential mode.    ---*/

  map<int,int>::const_iterator MI;
#ifdef HAVE_MPI

  /*--- Parallel mode. Send all the data using non-blocking sends. ---*/
  vector<MPI_Request> commReqs(3*nRankSend);
  MI = rankToIndCommBuf.begin();

  for(int i=0; i<nRankSend; ++i, ++MI) {

    int dest = MI->first;
    SU2_MPI::Isend(shortSendBuf[i].data(), shortSendBuf[i].size(), MPI_SHORT,
                   dest, dest, MPI_COMM_WORLD, &commReqs[3*i]);
    SU2_MPI::Isend(longSendBuf[i].data(), longSendBuf[i].size(), MPI_LONG,
                   dest, dest+1, MPI_COMM_WORLD, &commReqs[3*i+1]);
    SU2_MPI::Isend(doubleSendBuf[i].data(), doubleSendBuf[i].size(), MPI_DOUBLE,
                   dest, dest+2, MPI_COMM_WORLD, &commReqs[3*i+2]);
  }

  /* Loop over the number of ranks from which I receive data. */
  for(int i=0; i<nRankRecv; ++i) {

    /* Block until a message with shorts arrives from any processor.
       Determine the source and the size of the message.   */
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, rank, MPI_COMM_WORLD, &status);
    int source = status.MPI_SOURCE;

    int sizeMess;
    MPI_Get_count(&status, MPI_SHORT, &sizeMess);

    /* Allocate the memory for the short receive buffer and receive the message. */
    shortRecvBuf[i].resize(sizeMess);
    SU2_MPI::Recv(shortRecvBuf[i].data(), sizeMess, MPI_SHORT,
                  source, rank, MPI_COMM_WORLD, &status);

    /* Block until the corresponding message with longs arrives, determine
       its size, allocate the memory and receive the message. */
    MPI_Probe(source, rank+1, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_LONG, &sizeMess);
    longRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(longRecvBuf[i].data(), sizeMess, MPI_LONG,
                  source, rank+1, MPI_COMM_WORLD, &status);

    /* Idem for the message with doubles. */
    MPI_Probe(source, rank+2, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_DOUBLE, &sizeMess);
    doubleRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(doubleRecvBuf[i].data(), sizeMess, MPI_DOUBLE,
                  source, rank+2, MPI_COMM_WORLD, &status);
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(3*nRankSend, commReqs.data(), MPI_STATUSES_IGNORE);

  /* Wild cards have been used in the communication,
     so synchronize the ranks to avoid problems.    */
  MPI_Barrier(MPI_COMM_WORLD);

#else

  /*--- Sequential mode. Simply copy the buffers. ---*/
  shortRecvBuf[0]  = shortSendBuf[0];
  longRecvBuf[0]   = longSendBuf[0];
  doubleRecvBuf[0] = doubleSendBuf[0];

#endif

  /*--- Release the memory of the send buffers. To make sure that all
        the memory is deleted, the swap function is used. ---*/
  for(int i=0; i<nRankSend; ++i) {
    vector<short>().swap(shortSendBuf[i]);
    vector<long>().swap(longSendBuf[i]);
    vector<su2double>().swap(doubleSendBuf[i]);
  }

  /*--- Allocate the memory for the number of elements for every boundary
        marker and initialize them to zero.     ---*/
  nElem_Bound = new unsigned long[nMarker];
  for(unsigned short i=0; i<nMarker; ++i)
    nElem_Bound[i] = 0;

  /*--- Determine the global element ID's of the elements stored on this rank.
        Sort them in increasing order, such that an easy search can be done.
        In the same loop determine the upper bound for the local nodes (without
        halos) and the number of boundary elements for every marker. ---*/
  nElem = nPoint = 0;
  for(int i=0; i<nRankRecv; ++i) nElem += longRecvBuf[i][0];

  vector<unsigned long> globalElemID;
  globalElemID.reserve(nElem);

  for(int i=0; i<nRankRecv; ++i) {
    unsigned long indL = 1, indS = 0;
    for(long j=0; j<longRecvBuf[i][0]; ++j) {
      globalElemID.push_back(longRecvBuf[i][indL]);

      unsigned short nDOFsGrid = shortRecvBuf[i][indS+3];
      unsigned short nFaces    = shortRecvBuf[i][indS+5];
      indS += 2*nFaces + 7;
      indL += nDOFsGrid + nFaces + 2;
    }

    long nNodesThisRank = longRecvBuf[i][indL];
    nPoint += nNodesThisRank;
    indL   += nNodesThisRank+1;

    for(unsigned iMarker=0; iMarker<nMarker; ++iMarker) {
      long nBoundElemThisRank = longRecvBuf[i][indL]; ++indL;
      nElem_Bound[iMarker] += nBoundElemThisRank;

      for(long j=0; j<nBoundElemThisRank; ++j) {
        short nDOFsBoundElem = shortRecvBuf[i][indS+2];
        indS += 3;
        indL += nDOFsBoundElem + 2;
      }
    }
  }

  sort(globalElemID.begin(), globalElemID.end());

  /*--- Determine the global element ID's of the halo elements. A vector of
        unsignedLong2T is used for this purpose, such that a possible periodic
        transformation can be taken into account. Neighbors with a periodic
        transformation will always become a halo element, even if the element
        is stored on this rank.             ---*/
  vector<unsignedLong2T> haloElements;

  for(int i=0; i<nRankRecv; ++i) {
    unsigned long indL = 1, indS = 0;
    for(long j=0; j<longRecvBuf[i][0]; ++j) {
      unsigned short nDOFsGrid = shortRecvBuf[i][indS+3];
      unsigned short nFaces    = shortRecvBuf[i][indS+5];

      indS += 7;
      indL += nDOFsGrid + 2;
      for(unsigned short k=0; k<nFaces; ++k, indS+=2, ++indL) {
        if(longRecvBuf[i][indL] != -1) {
          bool neighborIsInternal = false;
          if(shortRecvBuf[i][indS] == -1) {
            neighborIsInternal = binary_search(globalElemID.begin(),
                                               globalElemID.end(),
                                               longRecvBuf[i][indL]);
          }

          if( !neighborIsInternal )
            haloElements.push_back(unsignedLong2T(longRecvBuf[i][indL],
                                                  shortRecvBuf[i][indS]+1));
        }
      }
    }
  }

  sort(haloElements.begin(), haloElements.end());
  vector<unsignedLong2T>::iterator lastHalo = unique(haloElements.begin(), haloElements.end());
  haloElements.erase(lastHalo, haloElements.end());

  /*----------------------------------------------------------------------------*/
  /*--- Step 2: Store the elements, nodes and boundary elements in the data  ---*/
  /*---         structures used by the FEM solver.                           ---*/
  /*----------------------------------------------------------------------------*/

  /* Determine the mapping from the global element number to the local entry.
     At the moment the sequence is based on the global element ID, but one can
     change this in order to have a better load balance for the threads.
     However, the owned elements should always be stored before the halos. */
  map<unsigned long,  unsigned long> mapGlobalElemIDToInd;
  map<unsignedLong2T, unsigned long> mapGlobalHaloElemToInd;

  nVolElemOwned = globalElemID.size();
  nVolElemTot   = nVolElemOwned + haloElements.size();

  for(unsigned long i=0; i<nVolElemOwned; ++i)
    mapGlobalElemIDToInd[globalElemID[i]] = i;

  for(unsigned long i=0; i<haloElements.size(); ++i)
    mapGlobalHaloElemToInd[haloElements[i]] = nVolElemOwned + i;

  /*--- Allocate the memory for the volume elements, the nodes
        and the surface elements of the boundaries.    ---*/
  volElem.resize(nVolElemTot);
  meshPoints.reserve(nPoint);

  boundaries.resize(nMarker);
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {
    boundaries[iMarker].markerTag        = config->GetMarker_All_TagBound(iMarker);
    boundaries[iMarker].periodicBoundary = config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY;
    boundaries[iMarker].surfElem.reserve(nElem_Bound[iMarker]);
  }

  /*--- Copy the data from the communication buffers. ---*/
  for(int i=0; i<nRankRecv; ++i) {

    /* The data for the volume elements. Loop over these elements in the buffer. */
    unsigned long indL = 1, indS = 0, indD = 0;
    for(long j=0; j<longRecvBuf[i][0]; ++j) {

      /* Determine the location in volElem where this data must be stored. */
      unsigned long elemID = longRecvBuf[i][indL++];
      map<unsigned long, unsigned long>::iterator MMI = mapGlobalElemIDToInd.find(elemID);
      unsigned long ind = MMI->second;

      /* Store the data. */
      volElem[ind].elemIsOwned        = true;
      volElem[ind].rankOriginal       = rank;
      volElem[ind].periodIndexToDonor = -1;

      volElem[ind].VTK_Type  = shortRecvBuf[i][indS++];
      volElem[ind].nPolyGrid = shortRecvBuf[i][indS++];
      volElem[ind].nPolySol  = shortRecvBuf[i][indS++];
      volElem[ind].nDOFsGrid = shortRecvBuf[i][indS++];
      volElem[ind].nDOFsSol  = shortRecvBuf[i][indS++];
      volElem[ind].nFaces    = shortRecvBuf[i][indS++];

      volElem[ind].JacIsConsideredConstant = (bool) shortRecvBuf[i][indS++];

      volElem[ind].elemIDGlobal        = elemID;
      volElem[ind].offsetDOFsSolGlobal = longRecvBuf[i][indL++];

      volElem[ind].nodeIDsGrid.resize(volElem[ind].nDOFsGrid);
      volElem[ind].JacFacesIsConsideredConstant.resize(volElem[ind].nFaces);

      for(unsigned short k=0; k<volElem[ind].nDOFsGrid; ++k)
        volElem[ind].nodeIDsGrid[k] = longRecvBuf[i][indL++];

      /* Update the counter indL with nFaces, because at this location in
         the long buffer the global ID of the neighboring element is stored.
         This data is not stored in volElem. */
      indL += volElem[ind].nFaces;

      for(unsigned short k=0; k<volElem[ind].nFaces; ++k) {
        ++indS; // At this location the periodic index of the face is stored in
                // shortRecvBuf, which is not stored in volElem.
        volElem[ind].JacFacesIsConsideredConstant[k] = (bool) shortRecvBuf[i][indS++];
      }
    }

    /* The data for the nodes. Loop over these nodes in the buffer and store
       them in meshPoints.             */
    unsigned long nNodesThisRank = longRecvBuf[i][indL++];
    for(unsigned long j=0; j<nNodesThisRank; ++j) {
      CPointFEM thisPoint;
      thisPoint.globalID = longRecvBuf[i][indL++];
      thisPoint.periodIndexToDonor = -1;
      for(unsigned short k=0; k<nDim; ++k)
        thisPoint.coor[k] = doubleRecvBuf[i][indD++];

      meshPoints.push_back(thisPoint);
    }

    /* The data for the boundary markers. Loop over them. */
    for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {

      unsigned long nElemThisRank = longRecvBuf[i][indL++];
      for(unsigned long j=0; j<nElemThisRank; ++j) {
        CSurfaceElementFEM thisSurfElem;

        thisSurfElem.VTK_Type  = shortRecvBuf[i][indS++];
        thisSurfElem.nPolyGrid = shortRecvBuf[i][indS++];
        thisSurfElem.nDOFsGrid = shortRecvBuf[i][indS++];

        thisSurfElem.volElemID         = longRecvBuf[i][indL++];
        thisSurfElem.boundElemIDGlobal = longRecvBuf[i][indL++];

        thisSurfElem.nodeIDsGrid.resize(thisSurfElem.nDOFsGrid);
        for(unsigned short k=0; k<thisSurfElem.nDOFsGrid; ++k)
          thisSurfElem.nodeIDsGrid[k] = longRecvBuf[i][indL++];

        boundaries[iMarker].surfElem.push_back(thisSurfElem);
      }
    }
  }

  /* Sort meshPoints in increasing order and remove the double entities. */
  sort(meshPoints.begin(), meshPoints.end());
  vector<CPointFEM>::iterator lastPoint = unique(meshPoints.begin(), meshPoints.end());
  meshPoints.erase(lastPoint, meshPoints.end());

  /*--- All the data from the receive buffers has been copied in the local
        data structures. Release the memory of the receive buffers. To make
        sure that all the memory is deleted, the swap function is used. ---*/
  for(int i=0; i<nRankRecv; ++i) {
    vector<short>().swap(shortRecvBuf[i]);
    vector<long>().swap(longRecvBuf[i]);
    vector<su2double>().swap(doubleRecvBuf[i]);
  }

  /*--- Sort the surface elements of the boundaries in increasing order. ---*/
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker)
    sort(boundaries[iMarker].surfElem.begin(), boundaries[iMarker].surfElem.end());

  /*----------------------------------------------------------------------------*/
  /*--- Step 3: Communicate the information for the halo elements.           ---*/
  /*----------------------------------------------------------------------------*/

  /* Determine the number of elements per rank of the originally partitioned grid
     stored in cumulative storage format. */
  vector<unsigned long> nElemPerRankOr(nRank+1);

  for(int i=0; i<nRank; ++i) nElemPerRankOr[i] = geometry->starting_node[i];
  nElemPerRankOr[nRank] = geometry->ending_node[nRank-1];

  /* Determine to which ranks I have to send messages to find out the information
     of the halos stored on this rank. */
  sendToRank.assign(nRank, 0);

  for(unsigned long i=0; i<haloElements.size(); ++i) {

    /* Determine the rank where this halo element was originally stored. */
    vector<unsigned long>::iterator low;
    low = lower_bound(nElemPerRankOr.begin(), nElemPerRankOr.end(),
                      haloElements[i].long0);
    unsigned long rankHalo = low - nElemPerRankOr.begin() -1;
    if(*low == haloElements[i].long0) ++rankHalo;

    sendToRank[rankHalo] = 1;
  }

  rankToIndCommBuf.clear();
  for(int i=0; i<nRank; ++i) {
    if( sendToRank[i] ) {
      int ind = rankToIndCommBuf.size();
      rankToIndCommBuf[i] = ind;
    }
  }

  /* Resize the first index of the long send buffers for the communication of
     the halo data.        */
  nRankSend = rankToIndCommBuf.size();
  longSendBuf.resize(nRankSend);

  /*--- Determine the number of ranks, from which this rank will receive elements. ---*/
  nRankRecv = nRankSend;

#ifdef HAVE_MPI
  MPI_Reduce_scatter(sendToRank.data(), &nRankRecv, sizeRecv.data(),
                     MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif

  /*--- Loop over the local halo elements to fill the communication buffers. ---*/
  for(unsigned long i=0; i<haloElements.size(); ++i) {

    /* Determine the rank where this halo element was originally stored. */
    vector<unsigned long>::iterator low;
    low = lower_bound(nElemPerRankOr.begin(), nElemPerRankOr.end(),
                      haloElements[i].long0);
    unsigned long ind = low - nElemPerRankOr.begin() -1;
    if(*low == haloElements[i].long0) ++ind;

    /* Convert this rank to the index in the send buffer. */
    MI = rankToIndCommBuf.find(ind);
    ind = MI->second;

    /* Store the global element ID and the periodic index in the long buffer.
       The subtraction of 1 is there to obtain the correct periodic index.
       In haloElements a +1 is added, because this variable is of unsigned long,
       which cannot handle negative numbers. */
    long perIndex = haloElements[i].long1 -1;

    longSendBuf[ind].push_back(haloElements[i].long0);
    longSendBuf[ind].push_back(perIndex);

    /* Determine the index in volElem where this halo must be stored. This info
       is also communicated, such that the return information can be stored in
       the correct location in volElem. */
    map<unsignedLong2T, unsigned long>::const_iterator MMI;
    MMI = mapGlobalHaloElemToInd.find(haloElements[i]);

    longSendBuf[ind].push_back(MMI->second);
  }

  /*--- Resize the first index of the long receive buffer. ---*/
  longRecvBuf.resize(nRankRecv);

  /*--- Communicate the data to the correct ranks. Make a distinction
        between parallel and sequential mode.    ---*/

#ifdef HAVE_MPI

  /* Parallel mode. Send all the data using non-blocking sends. */
  commReqs.resize(nRankSend);
  MI = rankToIndCommBuf.begin();

  for(int i=0; i<nRankSend; ++i, ++MI) {
    int dest = MI->first;
    SU2_MPI::Isend(longSendBuf[i].data(), longSendBuf[i].size(), MPI_LONG,
                   dest, dest, MPI_COMM_WORLD, &commReqs[i]);
  }

  /* Define the vector to store the ranks from which the message came. */
  vector<int> sourceRank(nRankRecv);

  /* Loop over the number of ranks from which I receive data. */
  for(int i=0; i<nRankRecv; ++i) {

    /* Block until a message with longs arrives from any processor.
       Determine the source and the size of the message and receive it. */
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, rank, MPI_COMM_WORLD, &status);
    sourceRank[i] = status.MPI_SOURCE;

    int sizeMess;
    MPI_Get_count(&status, MPI_LONG, &sizeMess);

    longRecvBuf[i].resize(sizeMess);
    SU2_MPI::Recv(longRecvBuf[i].data(), sizeMess, MPI_LONG,
                  sourceRank[i], rank, MPI_COMM_WORLD, &status);
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(nRankSend, commReqs.data(), MPI_STATUSES_IGNORE);

#else

  /*--- Sequential mode. Simply copy the buffer. ---*/
  longRecvBuf[0] = longSendBuf[0];

#endif

  /*--- Release the memory of the send buffers. To make sure that all the memory
        is deleted, the swap function is used. Afterwards resize the first index
        of the send buffers to nRankRecv, because this number of messages must
        be sent back to the sending ranks with halo information. ---*/
  for(int i=0; i<nRankSend; ++i) {
    vector<long>().swap(longSendBuf[i]);
  }

  shortSendBuf.resize(nRankRecv);
  longSendBuf.resize(nRankRecv);
  doubleSendBuf.resize(nRankRecv);

#ifdef HAVE_MPI
  /* Resize the vector of the communication requests to the number of messages
     to be sent ny this rank. Only in parallel node. */
  commReqs.resize(3*nRankRecv);
#endif

  /*--- Loop over the receive buffers to fill and send the send buffers again. ---*/
  for(int i=0; i<nRankRecv; ++i) {

    /* Vector with node IDs that must be returned to this calling rank.
       Note that also the periodic index must be stored, hence use an
       unsignedLong2T for this purpose. As -1 cannot be stored for an
       unsigned long a 1 is added to the periodic transformation.     */
    vector<unsignedLong2T> nodeIDs;

    /* Determine the number of elements present in longRecvBuf[i] and loop over
       them. Note that in position 0 of longSendBuf the number of elements
       present in communication buffers is stored. */
    long nElemBuf = longRecvBuf[i].size()/3;
    longSendBuf[i].push_back(nElemBuf);
    unsigned long indL = 0;
    for(long j=0; j<nElemBuf; ++j) {

      /* Determine the local index of the element in the original partitioning.
         Check if the index is valid. */
      long locElemInd = longRecvBuf[i][indL] - geometry->starting_node[rank];
      if(locElemInd < 0 || locElemInd >= (long) geometry->npoint_procs[rank]) {
        cout << locElemInd << " " << geometry->npoint_procs[rank] << endl;
        cout << "Invalid local element ID in function CMeshFEM::CMeshFEM" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }

      /* Store the periodic index in the short send buffer and the global element
         ID and local element ID (on the calling processor) in the long buffer. */
      longSendBuf[i].push_back(longRecvBuf[i][indL++]);
      short perIndex = (short) longRecvBuf[i][indL++];
      shortSendBuf[i].push_back(perIndex);
      longSendBuf[i].push_back(longRecvBuf[i][indL++]);

      /* Store the relevant information of this element in the short and long
         communication buffers. */
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetVTK_Type());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNPolyGrid());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNPolySol());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNDOFsGrid());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetNDOFsSol());
      shortSendBuf[i].push_back(geometry->elem[locElemInd]->GetnFaces());

      longSendBuf[i].push_back(geometry->elem[locElemInd]->GetColor());

      for(unsigned short j=0; j<geometry->elem[locElemInd]->GetNDOFsGrid(); ++j) {
        long thisNodeID = geometry->elem[locElemInd]->GetNode(j);
        longSendBuf[i].push_back(thisNodeID);
        nodeIDs.push_back(unsignedLong2T(thisNodeID,perIndex+1)); // Note the +1.
      }

      for(unsigned short j=0; j<geometry->elem[locElemInd]->GetnFaces(); ++j) {
        shortSendBuf[i].push_back((short) geometry->elem[locElemInd]->GetJacobianConstantFace(j));
      }
    }

    /*--- Sort nodeIDs in increasing order and remove the double entities. ---*/
    sort(nodeIDs.begin(), nodeIDs.end());
    vector<unsignedLong2T>::iterator lastNodeID = unique(nodeIDs.begin(), nodeIDs.end());
    nodeIDs.erase(lastNodeID, nodeIDs.end());

    /*--- Add the number of node IDs and the node IDs itself to longSendBuf[i]
          and the periodix index to shortSendBuf. Note again the -1 for the
          periodic index, because an unsigned long cannot represent -1, the
          value for the periodic index when no peridicity is present.       ---*/
    longSendBuf[i].push_back(nodeIDs.size());
    for(unsigned long j=0; j<nodeIDs.size(); ++j) {
      longSendBuf[i].push_back(nodeIDs[j].long0);
      shortSendBuf[i].push_back( (short) nodeIDs[j].long1-1);
    }

    /*--- Copy the coordinates to doubleSendBuf. ---*/
    for(unsigned long j=0; j<nodeIDs.size(); ++j) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalPointIDToLocalInd.find(nodeIDs[j].long0);

      if(LMI == globalPointIDToLocalInd.end()) {
        cout << "Entry not found in map in function CMeshFEM::CMeshFEM" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }

      unsigned long ind = LMI->second;
      for(unsigned short l=0; l<nDim; ++l)
        doubleSendBuf[i].push_back(geometry->node[ind]->GetCoord(l));
    }

    /* Release the memory of this receive buffer. */
    vector<long>().swap(longRecvBuf[i]);

    /*--- Send the communication buffers back to the calling rank.
          Only in parallel mode of course.     ---*/
#ifdef HAVE_MPI
    int dest = sourceRank[i];
    SU2_MPI::Isend(shortSendBuf[i].data(), shortSendBuf[i].size(), MPI_SHORT,
                   dest, dest+1, MPI_COMM_WORLD, &commReqs[3*i]);
    SU2_MPI::Isend(longSendBuf[i].data(), longSendBuf[i].size(), MPI_LONG,
                   dest, dest+2, MPI_COMM_WORLD, &commReqs[3*i+1]);
    SU2_MPI::Isend(doubleSendBuf[i].data(), doubleSendBuf[i].size(), MPI_DOUBLE,
                   dest, dest+3, MPI_COMM_WORLD, &commReqs[3*i+2]);
#endif
  }

  /*--- Resize the first index of the receive buffers to nRankSend, such that
        the requested halo information can be received.     ---*/
  shortRecvBuf.resize(nRankSend);
  longRecvBuf.resize(nRankSend);
  doubleRecvBuf.resize(nRankSend);

  /*--- Receive the communication data from the correct ranks. Make a distinction
        between parallel and sequential mode.    ---*/
#ifdef HAVE_MPI

  /* Parallel mode. Loop over the number of ranks from which I receive data
     in the return communication, i.e. nRankSend. */
  for(int i=0; i<nRankSend; ++i) {

    /* Block until a message with shorts arrives from any processor.
       Determine the source and the size of the message.   */
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, rank+1, MPI_COMM_WORLD, &status);
    int source = status.MPI_SOURCE;

    int sizeMess;
    MPI_Get_count(&status, MPI_SHORT, &sizeMess);

    /* Allocate the memory for the short receive buffer and receive the message. */
    shortRecvBuf[i].resize(sizeMess);
    SU2_MPI::Recv(shortRecvBuf[i].data(), sizeMess, MPI_SHORT,
                  source, rank+1, MPI_COMM_WORLD, &status);

    /* Block until the corresponding message with longs arrives, determine
       its size, allocate the memory and receive the message. */
    MPI_Probe(source, rank+2, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_LONG, &sizeMess);
    longRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(longRecvBuf[i].data(), sizeMess, MPI_LONG,
                  source, rank+2, MPI_COMM_WORLD, &status);

    /* Idem for the message with doubles. */
    MPI_Probe(source, rank+3, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_DOUBLE, &sizeMess);
    doubleRecvBuf[i].resize(sizeMess);

    SU2_MPI::Recv(doubleRecvBuf[i].data(), sizeMess, MPI_DOUBLE,
                  source, rank+3, MPI_COMM_WORLD, &status);
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(3*nRankRecv, commReqs.data(), MPI_STATUSES_IGNORE);

  /* Wild cards have been used in the communication,
     so synchronize the ranks to avoid problems.    */
  MPI_Barrier(MPI_COMM_WORLD);

#else

  /*--- Sequential mode. Simply copy the buffers. ---*/
  shortRecvBuf[0]  = shortSendBuf[0];
  longRecvBuf[0]   = longSendBuf[0];
  doubleRecvBuf[0] = doubleSendBuf[0];

#endif

  /*--- Release the memory of the send buffers. To make sure that all
        the memory is deleted, the swap function is used. ---*/
  for(int i=0; i<nRankRecv; ++i) {
    vector<short>().swap(shortSendBuf[i]);
    vector<long>().swap(longSendBuf[i]);
    vector<su2double>().swap(doubleSendBuf[i]);
  }

  /*----------------------------------------------------------------------------*/
  /*--- Step 4: Build the layer of halo elements from the information in the ---*/
  /*---         receive buffers shortRecvBuf, longRecvBuf and doubleRecvBuf. ---*/
  /*----------------------------------------------------------------------------*/

  /*--- Loop over the receive buffers to store the information of the
        halo elements and the halo points. ---*/
  vector<CPointFEM> haloPoints;
  for(int i=0; i<nRankSend; ++i) {

    /* Initialization of the indices in the communication buffers. */
    unsigned long indL = 1, indS = 0, indD = 0;

    /* Loop over the halo elements received from this rank. */
    for(long j=0; j<longRecvBuf[i][0]; ++j) {

      /* Retrieve the data from the communication buffers. */
      long globElemID = longRecvBuf[i][indL++];
      long indV       = longRecvBuf[i][indL++];

      volElem[indV].elemIDGlobal = globElemID;
      volElem[indV].rankOriginal = longRecvBuf[i][indL++];

      volElem[indV].periodIndexToDonor = shortRecvBuf[i][indS++];
      volElem[indV].VTK_Type           = shortRecvBuf[i][indS++];
      volElem[indV].nPolyGrid          = shortRecvBuf[i][indS++];
      volElem[indV].nPolySol           = shortRecvBuf[i][indS++];
      volElem[indV].nDOFsGrid          = shortRecvBuf[i][indS++];
      volElem[indV].nDOFsSol           = shortRecvBuf[i][indS++];
      volElem[indV].nFaces             = shortRecvBuf[i][indS++];

      volElem[indV].nodeIDsGrid.resize(volElem[indV].nDOFsGrid);
      for(unsigned short k=0; k<volElem[indV].nDOFsGrid; ++k)
        volElem[indV].nodeIDsGrid[k] = longRecvBuf[i][indL++];

      volElem[indV].JacFacesIsConsideredConstant.resize(volElem[indV].nFaces);
      for(unsigned short k=0; k<volElem[indV].nFaces; ++k)
        volElem[indV].JacFacesIsConsideredConstant[k] = (bool) shortRecvBuf[i][indS++];

      /* Give the member variables that are not obtained via communication their
         values. Some of these variables are not used for halo elements. */
      volElem[indV].elemIsOwned             = false;
      volElem[indV].JacIsConsideredConstant = false;
      volElem[indV].offsetDOFsSolGlobal     = ULONG_MAX;
    }

    /* Store the information of the points in haloPoints. */
    long nPointsThisRank = longRecvBuf[i][indL++];
    for(long j=0; j<nPointsThisRank; ++j) {
      CPointFEM thisPoint;
      thisPoint.globalID           = longRecvBuf[i][indL++];
      thisPoint.periodIndexToDonor = shortRecvBuf[i][indS++];
      for(unsigned short l=0; l<nDim; ++l)
        thisPoint.coor[l] = doubleRecvBuf[i][indD++];

      haloPoints.push_back(thisPoint);
    }

    /* The communication buffers from this rank are not needed anymore.
       Delete them using the swap function. */
    vector<short>().swap(shortRecvBuf[i]);
    vector<long>().swap(longRecvBuf[i]);
    vector<su2double>().swap(doubleRecvBuf[i]);
  }

  /* Remove the duplicate entries from haloPoints. */
  sort(haloPoints.begin(), haloPoints.end());
  lastPoint = unique(haloPoints.begin(), haloPoints.end());
  haloPoints.erase(lastPoint, haloPoints.end());

  /* Initialization of some variables to sort the halo points. */
  Global_nPoint = geometry->GetGlobal_nPoint();
  unsigned long InvalidPointID = Global_nPoint + 10;
  short         InvalidPerInd  = SHRT_MAX;

  /*--- Search for the nonperiodic halo points in the local points to see
        of these points are already stored on this rank. If this is the
        case invalidate this halo and decrease the number of halo points.
        Afterwards remove the invalid halos from the vector.       ---*/
  unsigned long nHaloPoints = haloPoints.size();
  for(unsigned long i=0; i<haloPoints.size(); ++i) {
    if(haloPoints[i].periodIndexToDonor != -1) break;  // Test for nonperiodic.

    if( binary_search(meshPoints.begin(), meshPoints.end(), haloPoints[i]) ) {
      haloPoints[i].globalID           = InvalidPointID;
      haloPoints[i].periodIndexToDonor = InvalidPerInd;
      --nHaloPoints;
    }
  }

  sort(haloPoints.begin(), haloPoints.end());
  haloPoints.resize(nHaloPoints);

  /* Increase the capacity of meshPoints, such that the halo points can be
     stored in there as well. Note that in case periodic points are present
     this is an upper bound. Add the non-periodic halo points to meshPoints. */
  meshPoints.reserve(meshPoints.size() + nHaloPoints);

  for(unsigned long i=0; i<haloPoints.size(); ++i) {
    if(haloPoints[i].periodIndexToDonor != -1) break;  // Test for nonperiodic.

    meshPoints.push_back(haloPoints[i]);
  }

  /* Create a map from the global point ID and periodic index to the local
     index in the vector meshPoints. First store the points already present
     in meshPoints. */
  map<unsignedLong2T, unsigned long> mapGlobalPointIDToInd;
  for(unsigned long i=0; i<meshPoints.size(); ++i) {
    unsignedLong2T globIndAndPer;
    globIndAndPer.long0 = meshPoints[i].globalID;
    globIndAndPer.long1 = meshPoints[i].periodIndexToDonor+1;  // Note the +1 again.

    mapGlobalPointIDToInd[globIndAndPer] = i;
  }

  /*--- Convert the global indices in the boundary connectivities to local ones. ---*/
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {
    for(unsigned long i=0; i<boundaries[iMarker].surfElem.size(); ++i) {

      /* Convert the corresponding volume element from global to local. */
      map<unsigned long, unsigned long>::const_iterator LMI;
      LMI = mapGlobalElemIDToInd.find(boundaries[iMarker].surfElem[i].volElemID);
      boundaries[iMarker].surfElem[i].volElemID = LMI->second;

      /* Convert the global node ID's to local values. Note that for these node
         ID's no periodic transformation can be present. */
      for(unsigned short j=0; j<boundaries[iMarker].surfElem[i].nDOFsGrid; ++j) {
        unsignedLong2T searchItem(boundaries[iMarker].surfElem[i].nodeIDsGrid[j], 0);
        map<unsignedLong2T, unsigned long>::const_iterator LLMI;
        LLMI = mapGlobalPointIDToInd.find(searchItem);
        boundaries[iMarker].surfElem[i].nodeIDsGrid[j] = LLMI->second;
      }
    }
  }

  /*--- The only halo points that must be added the meshPoints are the periodic
        halo points. It must be checked whether or not the periodic points in
        haloPoints match with points in meshPoints. This is done below. ---*/
  for(unsigned long iLow=0; iLow<haloPoints.size(); ) {

    /* Determine the upper index for this periodic transformation. */
    unsigned long iUpp;
    for(iUpp=iLow+1; iUpp<haloPoints.size(); ++iUpp)
      if(haloPoints[iUpp].periodIndexToDonor != haloPoints[iLow].periodIndexToDonor) break;

    /* Check for a true periodic index. */
    short perIndex = haloPoints[iLow].periodIndexToDonor;
    if(perIndex != -1) {

      /* Easier storage of the surface elements. */
      vector<CSurfaceElementFEM> &surfElem = boundaries[perIndex].surfElem;

      /*--- Store the points of this local periodic boundary in a data structure
            that can be used for searching coordinates. ---*/
      vector<CPointCompare> pointsBoundary;
      vector<long> indInPointsBoundary(meshPoints.size(), -1);
      for(unsigned long j=0; j<surfElem.size(); ++j) {

        /* Determine the tolerance for equal points, which is a small value
           times the length scale of this surface element. */
        su2double tolElem = 1.e-4*surfElem[j].DetermineLengthScale(meshPoints);

        /* Loop over the nodes of this surface grid and update the points on
           this periodic boundary. */
        for(unsigned short k=0; k<surfElem[j].nDOFsGrid; ++k) {
          unsigned long nn = surfElem[j].nodeIDsGrid[k];

          if(indInPointsBoundary[nn] == -1) {
            /* Point is not stored yet in pointsBoundary. Do so now. */
            indInPointsBoundary[nn] = pointsBoundary.size();

            CPointCompare thisPoint;
            thisPoint.nDim           = nDim;
            thisPoint.nodeID         = nn;
            thisPoint.tolForMatching = tolElem;
            for(unsigned short l=0; l<nDim; ++l)
              thisPoint.coor[l] = meshPoints[nn].coor[l];

            pointsBoundary.push_back(thisPoint);
          }
          else {
            /* Point is already stored in pointsBoundary. Update the tolerance. */
            nn = indInPointsBoundary[nn];
            pointsBoundary[nn].tolForMatching = min(pointsBoundary[nn].tolForMatching, tolElem);
          }
        }
      }

      /* Sort pointsBoundary in increasing order, such that binary searches can
         be carried out later on. */
      sort(pointsBoundary.begin(), pointsBoundary.end());

      /* Get the data for the periodic transformation to the donor. */
      su2double *center = config->GetPeriodicRotCenter(config->GetMarker_All_TagBound(perIndex));
      su2double *angles = config->GetPeriodicRotAngles(config->GetMarker_All_TagBound(perIndex));
      su2double *trans  = config->GetPeriodicTranslation(config->GetMarker_All_TagBound(perIndex));

      /*--- Compute the rotation matrix and translation vector for the
            transformation from the donor. This is the transpose of the
            transformation to the donor.               ---*/

      /* Store (center-trans) as it is constant and will be added on. */
      su2double translation[] = {center[0] - trans[0],
                                 center[1] - trans[1],
                                 center[2] - trans[2]};

      /* Store angles separately for clarity. Compute sines/cosines. */
      su2double theta = angles[0];
      su2double phi   = angles[1];
      su2double psi   = angles[2];

      su2double cosTheta = cos(theta), cosPhi = cos(phi), cosPsi = cos(psi);
      su2double sinTheta = sin(theta), sinPhi = sin(phi), sinPsi = sin(psi);

      /* Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis, then z-axis. */
      su2double rotMatrix[3][3];
      rotMatrix[0][0] =  cosPhi*cosPsi;
      rotMatrix[0][1] =  cosPhi*sinPsi;
      rotMatrix[0][2] = -sinPhi;

      rotMatrix[1][0] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;
      rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;
      rotMatrix[1][2] = sinTheta*cosPhi;

      rotMatrix[2][0] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
      rotMatrix[2][1] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
      rotMatrix[2][2] = cosTheta*cosPhi;

      /* Loop over the halo points for this periodic transformation. */
      for(unsigned long i=iLow; i<iUpp; ++i) {

        /* Apply the periodic transformation to the coordinates
           stored in this halo point. */
        su2double dx =             haloPoints[i].coor[0] - center[0];
        su2double dy =             haloPoints[i].coor[1] - center[1];
        su2double dz = nDim == 3 ? haloPoints[i].coor[2] - center[2] : 0.0;

        haloPoints[i].coor[0] = rotMatrix[0][0]*dx + rotMatrix[0][1]*dy
                              + rotMatrix[0][2]*dz + translation[0];
        haloPoints[i].coor[1] = rotMatrix[1][0]*dx + rotMatrix[1][1]*dy
                              + rotMatrix[1][2]*dz + translation[1];
        haloPoints[i].coor[2] = rotMatrix[2][0]*dx + rotMatrix[2][1]*dy
                              + rotMatrix[2][2]*dz + translation[2];

        /* Create an object of the type CPointCompare, which can be used
           to search the points on the periodic boundary, pointsBoundary. */
        CPointCompare thisPoint;
        thisPoint.nDim   = nDim;
        thisPoint.nodeID = ULONG_MAX;
        thisPoint.tolForMatching = 1.e+10;   // Just a large value.
        for(unsigned short l=0; l<nDim; ++l)
          thisPoint.coor[l] = haloPoints[i].coor[l];

        /* Check if this point is present in pointsBoundary. */
        if( binary_search(pointsBoundary.begin(), pointsBoundary.end(), thisPoint) ) {

          /* This point is present on the boundary. Find its position and
             store it in the mapping to the local points in meshPoints. */
          vector<CPointCompare>::const_iterator periodicLow;
          periodicLow = lower_bound(pointsBoundary.begin(), pointsBoundary.end(), thisPoint);

          unsignedLong2T globIndAndPer;
          globIndAndPer.long0 = haloPoints[i].globalID;
          globIndAndPer.long1 = haloPoints[i].periodIndexToDonor+1;  // Note the +1 again.

          mapGlobalPointIDToInd[globIndAndPer] = periodicLow->nodeID;
        }
        else {

          /* This point is not present yet on this rank yet. Store it in the
             mapping to the local points in mesh points and create it. */
          unsignedLong2T globIndAndPer;
          globIndAndPer.long0 = haloPoints[i].globalID;
          globIndAndPer.long1 = haloPoints[i].periodIndexToDonor+1;  // Note the +1 again.

          mapGlobalPointIDToInd[globIndAndPer] = meshPoints.size();
          meshPoints.push_back(haloPoints[i]);
        }
      }
    }

    /* Set iLow to iUpp for the next periodic transformation. */
    iLow = iUpp;
  }

  /*--- Convert the global node numbering in the elements to a local numbering. ---*/
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    for(unsigned short j=0; j<volElem[i].nDOFsGrid; ++j) {
      unsignedLong2T searchItem(volElem[i].nodeIDsGrid[j],
                                volElem[i].periodIndexToDonor+1); // Again the +1.
      map<unsignedLong2T, unsigned long>::const_iterator LLMI;
      LLMI = mapGlobalPointIDToInd.find(searchItem);
      volElem[i].nodeIDsGrid[j] = LLMI->second;
    }
  }
}

void CMeshFEM::ComputeGradientsCoordinatesFace(const unsigned short nIntegration,
                                               const unsigned short nDOFs,
                                               const su2double      *matDerBasisInt,
                                               const unsigned long  *DOFs,
                                               su2double            *derivCoor) {

  /* Allocate the memory to store the values of dxdr, dydr, etc. When the
     MKL is used a specialized allocation is used to increase performance. */
  su2double *dxdrVec;
#ifdef HAVE_MKL
  dxdrVec = (su2double *) mkl_malloc(nIntegration*nDim*nDim*sizeof(su2double), 64);
#else
  vector<su2double> helpDxdrVec(nIntegration*nDim*nDim);
  dxdrVec = helpDxdrVec.data();
#endif

  /* Determine the gradients of the Cartesian coordinates w.r.t. the
     parametric coordinates. */
  ComputeGradientsCoorWRTParam(nIntegration, nDOFs, matDerBasisInt, DOFs, dxdrVec);

  /* Make a distinction between 2D and 3D to compute the derivatives drdx,
     drdy, etc. */
  switch( nDim ) {
    case 2: {
      /* 2D computation. Store the offset between the r and s derivatives. */
      const unsigned short off = 2*nIntegration;

      /* Loop over the integration points. */
      unsigned short ii = 0;
      for(unsigned short j=0; j<nIntegration; ++j) {

        /* Retrieve the values of dxdr, dydr, dxds and dyds from dxdrVec
           in this integration point. */
        const unsigned short jx = 2*j; const unsigned short jy = jx+1;
        const su2double dxdr = dxdrVec[jx],     dydr = dxdrVec[jy];
        const su2double dxds = dxdrVec[jx+off], dyds = dxdrVec[jy+off];

        /* Compute the inverse relations drdx, drdy, dsdx, dsdy. */
        const su2double Jinv = 1.0/(dxdr*dyds - dxds*dydr);

        derivCoor[ii++] =  dyds*Jinv;  // drdx
        derivCoor[ii++] = -dxds*Jinv;  // drdy
        derivCoor[ii++] = -dydr*Jinv;  // dsdx
        derivCoor[ii++] =  dxdr*Jinv;  // dsdy
      }
      break;
    }

    case 3: {
      /* 3D computation. Store the offset between the r and s and r and t derivatives. */
      const unsigned short offS = 3*nIntegration, offT = 6*nIntegration;

      /* Loop over the integration points. */
      unsigned short ii = 0;
      for(unsigned short j=0; j<nIntegration; ++j) {

        /* Retrieve the values of dxdr, dydr, dzdr, dxds, dyds, dzds, dxdt, dydt
           and dzdt from dxdrVec in this integration point. */
        const unsigned short jx = 3*j; const unsigned short jy = jx+1, jz = jx+2;
        const su2double dxdr = dxdrVec[jx],      dydr = dxdrVec[jy],      dzdr = dxdrVec[jz];
        const su2double dxds = dxdrVec[jx+offS], dyds = dxdrVec[jy+offS], dzds = dxdrVec[jz+offS];
        const su2double dxdt = dxdrVec[jx+offT], dydt = dxdrVec[jy+offT], dzdt = dxdrVec[jz+offT];

        /* Compute the inverse relations drdx, drdy, drdz, dsdx, dsdy, dsdz,
           dtdx, dtdy, dtdz. */
        const su2double Jinv = 1.0/(dxdr*(dyds*dzdt - dzds*dydt)
                             -      dxds*(dydr*dzdt - dzdr*dydt)
                             +      dxdt*(dydr*dzds - dzdr*dyds));

        derivCoor[ii++] = (dyds*dzdt - dzds*dydt)*Jinv;  // drdx
        derivCoor[ii++] = (dzds*dxdt - dxds*dzdt)*Jinv;  // drdy
        derivCoor[ii++] = (dxds*dydt - dyds*dxdt)*Jinv;  // drdz

        derivCoor[ii++] = (dzdr*dydt - dydr*dzdt)*Jinv;  // dsdx
        derivCoor[ii++] = (dxdr*dzdt - dzdr*dxdt)*Jinv;  // dsdy
        derivCoor[ii++] = (dydr*dxdt - dxdr*dydt)*Jinv;  // dsdz

        derivCoor[ii++] = (dydr*dzds - dzdr*dyds)*Jinv;  // dtdx
        derivCoor[ii++] = (dzdr*dxds - dxdr*dzds)*Jinv;  // dtdy
        derivCoor[ii++] = (dxdr*dyds - dydr*dxds)*Jinv;  // dtdz
      }

      break;
    }
  }

  /* Release the memory when the MKL is used. */
#ifdef HAVE_MKL
  mkl_free(dxdrVec);
#endif
}

void CMeshFEM::ComputeGradientsCoorWRTParam(const unsigned short nIntegration,
                                            const unsigned short nDOFs,
                                            const su2double      *matDerBasisInt,
                                            const unsigned long  *DOFs,
                                            su2double            *derivCoor) {

  /*--- Make a distinction whether BLAS routines must be used to carry
        out the multiplication or a standard implementation must be used. ---*/
#if defined (HAVE_CBLAS) || defined(HAVE_MKL)

  /* Allocate the memory to store the coordinates as right hand side. when
     the MKL is used, a specialized allocation is used to increase performance. */
  su2double *vecRHS;

#ifdef HAVE_MKL
  vecRHS = (su2double *) mkl_malloc(nDOFs*nDim*sizeof(su2double), 64);
#else
  vector<su2double> helpVecRHS(nDOFs*nDim);
  vecRHS = helpVecRHS.data();
#endif

  /* Loop over the grid DOFs of the element and copy the coordinates in
     vecRHS in row major order. */
  unsigned long ic = 0;
  for(unsigned short j=0; j<nDOFs; ++j) {
    for(unsigned short k=0; k<nDim; ++k, ++ic)
      vecRHS[ic] = meshPoints[DOFs[j]].coor[k];
  }

  /* Carry out the matrix matrix product using the blas routine dgemm. */
  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nDim*nIntegration, nDim,
              nDOFs, 1.0, matDerBasisInt, nDOFs, vecRHS, nDim, 0.0, derivCoor, nDim);

  /* Release the memory of vecRHS in case the MKL was used. */
#ifdef HAVE_MKL
  mkl_free(vecRHS);
#endif

#else

  /* Standard implementation of the matrix matrix multiplication. */
  const unsigned short m = nDim*nIntegration;

  for(unsigned short i=0; i<m; ++i) {
    const unsigned short jj = i*nDOFs;
    for(unsigned short j=0; j<nDim; ++j) {
      const unsigned short ii = i*nDim + j;
      derivCoor[ii] = 0.0;
      for(unsigned short k=0; k<nDOFs; ++k)
        derivCoor[ii] += matDerBasisInt[jj+k]*meshPoints[DOFs[k]].coor[j];
    }
  }

#endif
}

void CMeshFEM::ComputeMetricTermsSIP(const unsigned short nIntegration,
                                     const unsigned short nDOFs,
                                     const su2double      *dr,
                                     const su2double      *ds,
                                     const su2double      *dt,
                                     const su2double      *normals,
                                     const su2double      *derivCoor,
                                     su2double            *metricSIP) {

  /* Initialize the counter ii to 0. This counter is the index in metricSIP
     where the data is stored. */
  unsigned int ii = 0;

  /* Make a distinction between 2D and 3D. */
  switch( nDim ) {
    case 2: {
      /* 2D computation. Loop over the integration points. */
      for(unsigned short j=0; j<nIntegration; ++j) {

        /* Easier storage for the derivatives of the basis functions in this
           integration point. */
        const su2double *drr = &dr[j*nDOFs];
        const su2double *dss = &ds[j*nDOFs];

        /* Idem for the normals and derivCoor. */
        const su2double *norm  = &normals[3*j];   // j*(nDim+1)
        const su2double *dCoor = &derivCoor[4*j]; // j*nDim*nDim

        /* Loop over the DOFs. */
        for(unsigned short i=0; i<nDOFs; ++i, ++ii) {

          /* Compute the Cartesian derivatives of this basis function. */
          const su2double dldx = drr[i]*dCoor[0] + dss[i]*dCoor[2];
          const su2double dldy = drr[i]*dCoor[1] + dss[i]*dCoor[3];

          /* Compute the SIP metric term for this DOF in this integration point. */
          metricSIP[ii] = norm[2]*(dldx*norm[0] + dldy*norm[1]);
        }
      }

      break;
    }

    case 3: {
      /* 3D computation. Loop over the integration points. */
      for(unsigned short j=0; j<nIntegration; ++j) {

        /* Easier storage for the derivatives of the basis functions in this
           integration point. */
        const su2double *drr = &dr[j*nDOFs];
        const su2double *dss = &ds[j*nDOFs];
        const su2double *dtt = &dt[j*nDOFs];

        /* Idem for the normals and derivCoor. */
        const su2double *norm  = &normals[4*j];   // j*(nDim+1)
        const su2double *dCoor = &derivCoor[9*j]; // j*nDim*nDim

        /* Loop over the DOFs. */
        for(unsigned short i=0; i<nDOFs; ++i, ++ii) {

          /* Compute the Cartesian derivatives of this basis function. */
          const su2double dldx = drr[i]*dCoor[0] + dss[i]*dCoor[3] + dtt[i]*dCoor[6];
          const su2double dldy = drr[i]*dCoor[1] + dss[i]*dCoor[4] + dtt[i]*dCoor[7];
          const su2double dldz = drr[i]*dCoor[2] + dss[i]*dCoor[5] + dtt[i]*dCoor[8];

          /* Compute the SIP metric term for this DOF in this integration point. */
          metricSIP[ii] = norm[3]*(dldx*norm[0] + dldy*norm[1] + dldz*norm[2]);
        }
      }
    }
  }
}

void CMeshFEM::ComputeNormalsFace(const unsigned short nIntegration,
                                  const unsigned short nDOFs,
                                  const su2double      *dr,
                                  const su2double      *ds,
                                  const unsigned long  *DOFs,
                                  su2double            *normals) {

  /* Initialize the counter ii to 0. ii is the index in normals where the
     information is stored. */
  unsigned int ii = 0;

  /* Make a distinction between 2D and 3D. */
  switch( nDim ) {
    case 2: {
      /* 2D computation. Loop over the integration points of the face. */
      for(unsigned short j=0; j<nIntegration; ++j) {

        /*--- Loop over the number of DOFs of the face to compute dxdr
              and dydr. ---*/
        const su2double *drr = &dr[j*nDOFs];
        su2double dxdr = 0.0, dydr = 0.0;
        for(unsigned short k=0; k<nDOFs; ++k) {
          dxdr += drr[k]*meshPoints[DOFs[k]].coor[0];
          dydr += drr[k]*meshPoints[DOFs[k]].coor[1];
        }

        /* Determine the length of the tangential vector (dxdr, dydr), which
           is also the length of the corresponding normal vector. Also compute
           the inverse of the length. Make sure that a division by zero is
           avoided, although this is most likely never active. */
        const su2double lenNorm    = sqrt(dxdr*dxdr + dydr*dydr);
        const su2double invLenNorm = lenNorm < 1.e-50 ? 1.e+50 : 1.0/lenNorm;

        /* Store the corresponding unit normal vector and its length. The
           direction of the normal vector is such that it is outward pointing
           for the element on side 0 of the face. */
        normals[ii++] =  dydr*invLenNorm;
        normals[ii++] = -dxdr*invLenNorm;
        normals[ii++] =  lenNorm;
      }

      break;
    }

    case 3: {
      /* 3D computation. Loop over the integration points of the face. */
      for(unsigned short j=0; j<nIntegration; ++j) {

        /*--- Loop over the number of DOFs of the face to compute dxdr,
              dxds, dydr, dyds, dzdr and dzds. ---*/
        const su2double *drr = &dr[j*nDOFs], *dss = &ds[j*nDOFs];
        su2double dxdr = 0.0, dydr = 0.0, dzdr = 0.0,
                  dxds = 0.0, dyds = 0.0, dzds = 0.0;
        for(unsigned short k=0; k<nDOFs; ++k) {
          dxdr += drr[k]*meshPoints[DOFs[k]].coor[0];
          dydr += drr[k]*meshPoints[DOFs[k]].coor[1];
          dzdr += drr[k]*meshPoints[DOFs[k]].coor[2];

          dxds += dss[k]*meshPoints[DOFs[k]].coor[0];
          dyds += dss[k]*meshPoints[DOFs[k]].coor[1];
          dzds += dss[k]*meshPoints[DOFs[k]].coor[2];
        }

        /* Compute the vector product dxdr X dxds, where x is the coordinate
           vector (x,y,z). Compute the length of this vector, which is an area,
           as well as the inverse.  Make sure that a division by zero is
           avoided, although this is most likely never active. */
        const su2double nx = dydr*dzds - dyds*dzdr;
        const su2double ny = dxds*dzdr - dxdr*dzds;
        const su2double nz = dxdr*dyds - dxds*dydr;

        const su2double lenNorm    = sqrt(nx*nx + ny*ny + nz*nz);
        const su2double invLenNorm = lenNorm < 1.e-50 ? 1.e+50 : 1.0/lenNorm;

        /* Store the components of the unit normal as well as its length in the
           normals. Note that the current direction of the normal is pointing
           into the direction of the element on side 0 of the face. However,
           in the actual computation of the integral over the faces, it is
           assumed that the vector points in the opposite direction.
           Hence the normal vector must be reversed. */
        normals[ii++] = -nx*invLenNorm;
        normals[ii++] = -ny*invLenNorm;
        normals[ii++] = -nz*invLenNorm;
        normals[ii++] =  lenNorm;
      }
      break;
    }
  }
}

void CMeshFEM::MetricTermsBoundaryFaces(CBoundaryFEM *boundary) {

  /*--------------------------------------------------------------------------*/
  /*--- Step 1: Determine the size of the vector, which stores the metric  ---*/
  /*---         terms of the boundary face elements. This is a large,      ---*/
  /*---         contiguous vector to increase the performance.             ---*/
  /*---         Each boundary face element has pointers, which point to    ---*/
  /*---         particular regions of the large vector.                     ---*/
  /*--------------------------------------------------------------------------*/

  /*--- Loop over the boundary faces stored on this rank and determine
        the size of the metric vector for the faces. Note that the normal
        gradient of the basis functions of the adjacent element must be stored.
        These terms enter the formulation in the Symmetric Interior Penalty
        (SIP) treatment of the viscous terms. ---*/
  unsigned long sizeMetric = 0;
  for(unsigned long i=0; i<boundary->surfElem.size(); ++i) {

    /* Determine the corresponding standard face element, the number of
       integration points for this face element and the number of DOFS
       of the adjacent volume element. */
    const unsigned short ind       = boundary->surfElem[i].indStandardElement;
    const unsigned short nInt      = standardBoundaryFacesSol[ind].GetNIntegration();
    const unsigned short nDOFsElem = standardBoundaryFacesSol[ind].GetNDOFsElem();

    /* Update the counter sizeMetric by adding the required number for
       this face element. For each integration point the following data
       is stored: - Unit normals + area (nDim+1).
                  - drdx, dsdx, etc. (nDim*nDim).
                  - Normal derivatives of the element basis functions
                    (nDOFsElem). */
    sizeMetric += nInt*(nDim + 1 + nDim*nDim + nDOFsElem);
  }

  /* Allocate the memory for the vector to store the metric terms.
     Get its pointer to improve readability. */
  boundary->VecMetricTermsBoundaryFaces.resize(sizeMetric);
  su2double *VecMetricBoundary = boundary->VecMetricTermsBoundaryFaces.data();

  /*--------------------------------------------------------------------------*/
  /*--- Step 2: Set the pointers for storing the metric terms to the       ---*/
  /*---         correct locations in VecMetricBoundary.                    ---*/
  /*--------------------------------------------------------------------------*/

  /* Loop over the boundary faces. */
  sizeMetric = 0;
  for(unsigned long i=0; i<boundary->surfElem.size(); ++i) {

    /* Determine the corresponding standard face element and get the
       relevant information from it. */
    const unsigned short ind       = boundary->surfElem[i].indStandardElement;
    const unsigned short nInt      = standardBoundaryFacesSol[ind].GetNIntegration();
    const unsigned short nDOFsElem = standardBoundaryFacesSol[ind].GetNDOFsElem();

    /* Set the pointers to the locations in VecMetricBoundary
       where the actual metric data for this boundary face is stored. */
    boundary->surfElem[i].metricNormalsFace = VecMetricBoundary + sizeMetric;
    sizeMetric += nInt*(nDim+1);

    boundary->surfElem[i].metricCoorDerivFace = VecMetricBoundary + sizeMetric;
    sizeMetric += nInt*nDim*nDim;

    boundary->surfElem[i].metricElem = VecMetricBoundary + sizeMetric;
    sizeMetric += nInt*nDOFsElem;
  }

  /*--------------------------------------------------------------------------*/
  /*--- Step 3: Determine the actual metric data in the integration points ---*/
  /*---         of the faces.                                              ---*/
  /*--------------------------------------------------------------------------*/

  /* Loop over the boundary faces. */
  for(unsigned long i=0; i<boundary->surfElem.size(); ++i) {

    /* Determine the corresponding standard face and its number of integration
       points. Note that the standard element of the grid must be used here. */
    const unsigned short ind  = boundary->surfElem[i].indStandardElement;
    const unsigned short nInt = standardBoundaryFacesGrid[ind].GetNIntegration();

    /* Call the function ComputeNormalsFace to compute the unit normals and
       its corresponding area in the integration points. */
    unsigned short nDOFs = standardBoundaryFacesGrid[ind].GetNDOFsFace();
    const su2double *dr = standardBoundaryFacesGrid[ind].GetDrBasisFaceIntegration();
    const su2double *ds = standardBoundaryFacesGrid[ind].GetDsBasisFaceIntegration();

    ComputeNormalsFace(nInt, nDOFs, dr, ds, boundary->surfElem[i].DOFsGridFace,
                       boundary->surfElem[i].metricNormalsFace);

    /* Compute the derivatives of the parametric coordinates w.r.t. the
       Cartesian coordinates, i.e. drdx, drdy, etc. in the integration points
       of the face. */
    nDOFs = standardBoundaryFacesGrid[ind].GetNDOFsElem();
    dr = standardBoundaryFacesGrid[ind].GetMatDerBasisElemIntegration();

    ComputeGradientsCoordinatesFace(nInt, nDOFs, dr,
                                    boundary->surfElem[i].DOFsGridElement,
                                    boundary->surfElem[i].metricCoorDerivFace);

    /* Compute the metric terms needed for the SIP treatment of the viscous
       terms. Note that now the standard element of the solution must be taken. */
    const su2double *dt;
    nDOFs = standardBoundaryFacesSol[ind].GetNDOFsElem();
    dr = standardBoundaryFacesSol[ind].GetDrBasisElemIntegration();
    ds = standardBoundaryFacesSol[ind].GetDsBasisElemIntegration();
    dt = standardBoundaryFacesSol[ind].GetDtBasisElemIntegration();

    ComputeMetricTermsSIP(nInt, nDOFs, dr, ds, dt,
                          boundary->surfElem[i].metricNormalsFace,
                          boundary->surfElem[i].metricCoorDerivFace,
                          boundary->surfElem[i].metricElem);
  }
}

CMeshFEM_DG::CMeshFEM_DG(CGeometry *geometry, CConfig *config)
  : CMeshFEM(geometry, config) {
}

void CMeshFEM_DG::CreateFaces(CConfig *config) {

  /*---------------------------------------------------------------------------*/
  /*--- Step 1: Determine the faces of the locally stored part of the grid. ---*/
  /*---------------------------------------------------------------------------*/

  /*--- Loop over the volume elements stored on this rank, including the halos. ---*/
  vector<FaceOfElementClass> localFaces;

  for(unsigned long k=0; k<nVolElemTot; ++k) {

    /* Determine the corner points of all the faces of this element. */
    unsigned short nFaces;
    unsigned short nPointsPerFace[6];
    unsigned long  faceConn[6][4];

    volElem[k].GetCornerPointsAllFaces(nFaces, nPointsPerFace, faceConn);

    /* Loop over the faces of this element, set the appropriate information,
       create a unique numbering and add the faces to localFaces. */
    for(unsigned short i=0; i<nFaces; ++i) {
      FaceOfElementClass thisFace;
      thisFace.nCornerPoints = nPointsPerFace[i];
      for(unsigned short j=0; j<nPointsPerFace[i]; ++j)
        thisFace.cornerPoints[j] = faceConn[i][j];

      thisFace.elemID0       = k;
      thisFace.nPolyGrid0    = volElem[k].nPolyGrid;
      thisFace.nPolySol0     = volElem[k].nPolySol;
      thisFace.nDOFsElem0    = volElem[k].nDOFsSol;
      thisFace.elemType0     = volElem[k].VTK_Type;
      thisFace.faceID0       = i;
      thisFace.faceIndicator = volElem[k].elemIsOwned ? -1 : -2;

      thisFace.JacFaceIsConsideredConstant = volElem[k].JacFacesIsConsideredConstant[i];

      thisFace.CreateUniqueNumberingWithOrientation();

      localFaces.push_back(thisFace);
    }
  }

  /*--- Sort the the local faces in increasing order. ---*/
  sort(localFaces.begin(), localFaces.end());

  /*--- Loop over the faces to merge the matching faces. As only one of the
        faces is kept, the other face is invalidated by setting its faceIndicator
        to -2, i.e. an unowned faces. In this way these faces can be removed
        easily later on. ---*/
  for(unsigned long i=1; i<localFaces.size(); ++i) {

    /* Check for a matching face with the previous face in the vector.
       Note that the == operator only checks the node IDs. */
    if(localFaces[i] == localFaces[i-1]) {

      /* Faces are matching. First check if at least one face belongs to an
         owned element. In that case the face should be stored, because it
         contributes to the surface integral of an owned element. */
      if(localFaces[i].faceIndicator == -1 || localFaces[i-1].faceIndicator == -1) {

        /* Store the of the neighboring elemen in faces[i-1]. */
        if(localFaces[i].elemID0 < nVolElemTot) {
          localFaces[i-1].elemID0    = localFaces[i].elemID0;
          localFaces[i-1].nPolyGrid0 = localFaces[i].nPolyGrid0;
          localFaces[i-1].nPolySol0  = localFaces[i].nPolySol0;
          localFaces[i-1].nDOFsElem0 = localFaces[i].nDOFsElem0;
          localFaces[i-1].elemType0  = localFaces[i].elemType0;
          localFaces[i-1].faceID0    = localFaces[i].faceID0;
        }
        else {
          localFaces[i-1].elemID1    = localFaces[i].elemID1;
          localFaces[i-1].nPolyGrid1 = localFaces[i].nPolyGrid1;
          localFaces[i-1].nPolySol1  = localFaces[i].nPolySol1;
          localFaces[i-1].nDOFsElem1 = localFaces[i].nDOFsElem1;
          localFaces[i-1].elemType1  = localFaces[i].elemType1;
          localFaces[i-1].faceID1    = localFaces[i].faceID1;
        }

        /* Adapt the boolean to indicate whether or not the face has a constant
           Jacobian of the transformation, although in principle this info
           should be the same for both faces. */
        if( !(localFaces[i-1].JacFaceIsConsideredConstant &&
              localFaces[i].JacFaceIsConsideredConstant) ) {
          localFaces[i-1].JacFaceIsConsideredConstant = false;
        }

        /* Set this face indicator to -1 to indicate that this face must be kept
           and invalidate localFace[i] by setting its face indicator to -2. */
        localFaces[i-1].faceIndicator = -1;
        localFaces[i].faceIndicator   = -2;
      }
    }
  }

  /*--- Remove the invalidated faces. This is accomplished by giving the
        face four points a global node ID that is larger than the largest
        local point ID in the grid. In this way the sorting operator puts
        these faces at the end of the vector, see also the < operator
        of FaceOfElementClass. ---*/
  unsigned long nFacesLoc = localFaces.size();
  for(unsigned long i=0; i<localFaces.size(); ++i) {
    if(localFaces[i].faceIndicator == -2) {
      unsigned long invalID = meshPoints.size();
      localFaces[i].nCornerPoints = 4;
      localFaces[i].cornerPoints[0] = invalID;
      localFaces[i].cornerPoints[1] = invalID;
      localFaces[i].cornerPoints[2] = invalID;
      localFaces[i].cornerPoints[3] = invalID;
      --nFacesLoc;
    }
  }

  sort(localFaces.begin(), localFaces.end());
  localFaces.resize(nFacesLoc);

  /*--- Loop over the boundary markers and its boundary elements to search for
        the corresponding faces in localFaces. These faces should be found.
        Note that periodic boundaries are skipped, because these are treated
        via the halo elements, which are already in place. ---*/
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {
    if( !boundaries[iMarker].periodicBoundary ) {

      for(unsigned long k=0; k<boundaries[iMarker].surfElem.size(); ++k) {

        /* Determine the corner points of the face of this element. */
        unsigned short nPointsPerFace;
        unsigned long  faceConn[4];

        boundaries[iMarker].surfElem[k].GetCornerPointsFace(nPointsPerFace, faceConn);

        /* Create an object of FaceOfElementClass to carry out the search. */
        FaceOfElementClass thisFace;
        thisFace.nCornerPoints = nPointsPerFace;
        for(unsigned short j=0; j<nPointsPerFace; ++j)
          thisFace.cornerPoints[j] = faceConn[j];
        thisFace.CreateUniqueNumberingWithOrientation();

        /* Search for thisFace in localFaces. It must be found. */
        if( binary_search(localFaces.begin(), localFaces.end(), thisFace) ) {
          vector<FaceOfElementClass>::iterator low;
          low = lower_bound(localFaces.begin(), localFaces.end(), thisFace);
          low->faceIndicator = iMarker;

          /* A few additional checks. */
          bool side0IsBoundary = low->elemID0 < nVolElemTot;
          unsigned long elemID = side0IsBoundary ? low->elemID0    : low->elemID1;
          unsigned short nPoly = side0IsBoundary ? low->nPolyGrid0 : low->nPolyGrid1;

          if(elemID != boundaries[iMarker].surfElem[k].volElemID ||
             nPoly  != boundaries[iMarker].surfElem[k].nPolyGrid) {
            cout << "Element ID and/or polynomial degree do not match "
                 << "for this boundary element. This should not happen." << endl;
#ifndef HAVE_MPI
            exit(EXIT_FAILURE);
#else
            MPI_Abort(MPI_COMM_WORLD,1);
            MPI_Finalize();
#endif
          }

          /* Store the local index of the boundary face in the variable for the
             polynomial degree, which is not used for in localFaces. */
          if( side0IsBoundary ) low->nPolyGrid1 = k;
          else                  low->nPolyGrid0 = k;
        }
        else {
          cout << "Boundary face not found in localFaces. "
               << "This should not happen." << endl;
#ifndef HAVE_MPI
          exit(EXIT_FAILURE);
#else
          MPI_Abort(MPI_COMM_WORLD,1);
          MPI_Finalize();
#endif
        }
      }
    }
  }

  /*---------------------------------------------------------------------------*/
  /*--- Step 2: Preparation of the localFaces vector, such that the info    ---*/
  /*---         stored in this vector can be separated in a contribution    ---*/
  /*---         from the internal faces and a contribution from the faces   ---*/
  /*---         that belong to physical boundaries.                         ---*/
  /*---------------------------------------------------------------------------*/

  /* Sort localFaces again, but now such that the boundary faces are numbered
     first, followed by the matching faces and at the end of localFaces the
     non-matching faces are stored. In order to carry out this sorting the
     functor SortFacesClass is used for comparison. */
  sort(localFaces.begin(), localFaces.end(), SortFacesClass(nVolElemTot));

  /*--- In order to reduce the number of standard elements for the matching
        faces, it is made sure that the VTK type of the element on side 0
        must be less than or equal to the VTK type of the element on side 1.
        Furthermore, make sure that for boundary faces and non-matching faces
        the corresponding element is always on side 0. ---*/
  for(unsigned long i=0; i<localFaces.size(); ++i) {

    /* Determine whether or not the ajacent elements must be swapped. */
    bool swapElements;
    if(localFaces[i].elemID0 < nVolElemTot &&
       localFaces[i].elemID1 < nVolElemTot) {

      /* This is an internal matching face. Check the VTK types of the
         adjacent elements. */
      if(localFaces[i].elemType0 == localFaces[i].elemType1) {

        /* The same element type on both sides. Make sure that the element
           with the smallest ID is stored on side 0 of the face. */
        swapElements = localFaces[i].elemID0 > localFaces[i].elemID1;
      }
      else {
        /* Different element types. Make sure that the lowest element
           type will be stored on side 0 of the face. */
        swapElements = localFaces[i].elemType0 > localFaces[i].elemType1;
      }
    }
    else {

      /* Either a boundary face or a non-matching face. It must be swapped
         if the element is currently on side 1 of the face. */
      swapElements = localFaces[i].elemID1 < nVolElemTot;
    }

    /* Swap the adjacent elements of the face, if needed. Note that
       also the sequence of the corner points must be altered in order
       to obey the right hand rule. */
    if( swapElements ) {
      swap(localFaces[i].elemID0,    localFaces[i].elemID1);
      swap(localFaces[i].nPolyGrid0, localFaces[i].nPolyGrid1);
      swap(localFaces[i].nPolySol0,  localFaces[i].nPolySol1);
      swap(localFaces[i].nDOFsElem0, localFaces[i].nDOFsElem1);
      swap(localFaces[i].elemType0,  localFaces[i].elemType1);
      swap(localFaces[i].faceID0,    localFaces[i].faceID1);

      if(localFaces[i].nCornerPoints == 2)
        swap(localFaces[i].cornerPoints[0], localFaces[i].cornerPoints[1]);
      else
        swap(localFaces[i].cornerPoints[0], localFaces[i].cornerPoints[2]);
    }
  }

  /*--- For triangular faces with a pyramid as an adjacent element, it    ---*/
  /*--- must be made sure that the first corner point does not coincide   ---*/
  /*--- with the top of the pyramid. Otherwise it is impossible to carry  ---*/
  /*--- the transformation to the standard pyramid element.               ---*/
  for(unsigned long i=0; i<localFaces.size(); ++i) {

    /* Check for a triangular face. */
    if(localFaces[i].nCornerPoints == 3) {

      /* Determine if the corner points correspond to a corner point of a
         pyramid. A pyramid can in principle occur on both sides of the face. */
      bool cornerIsTopPyramid[] = {false, false, false};

      if(localFaces[i].elemType0 == PYRAMID) {
        unsigned long topPyramid = volElem[localFaces[i].elemID0].nodeIDsGrid.back();
        if(localFaces[i].cornerPoints[0] == topPyramid) cornerIsTopPyramid[0] = true;
        if(localFaces[i].cornerPoints[1] == topPyramid) cornerIsTopPyramid[1] = true;
        if(localFaces[i].cornerPoints[2] == topPyramid) cornerIsTopPyramid[2] = true;
      }

      if(localFaces[i].elemType1 == PYRAMID) {
        unsigned long topPyramid = volElem[localFaces[i].elemID1].nodeIDsGrid.back();
        if(localFaces[i].cornerPoints[0] == topPyramid) cornerIsTopPyramid[0] = true;
        if(localFaces[i].cornerPoints[1] == topPyramid) cornerIsTopPyramid[1] = true;
        if(localFaces[i].cornerPoints[2] == topPyramid) cornerIsTopPyramid[2] = true;
      }

      /* Check if the first element corresponds to the top of a pyramid. */
      if( cornerIsTopPyramid[0] ) {

        /* The sequence of the points of the face must be altered. It is done in
           such a way that the orientation remains the same. Store the original
           point numbering. */
        unsigned long tmp[] = {localFaces[i].cornerPoints[0],
                               localFaces[i].cornerPoints[1],
                               localFaces[i].cornerPoints[2]};

        /* Determine the situation. */
        if( !cornerIsTopPyramid[1] ) {

          /* cornerPoint[1] is not a top of a pyramid. Hence this will become
             the point 0 of the triangle. */
          localFaces[i].cornerPoints[0] = tmp[1];
          localFaces[i].cornerPoints[1] = tmp[2];
          localFaces[i].cornerPoints[2] = tmp[0];
        }
        else {

          /* Only cornerPoint[2] is not a top of a pyramid. Hence this will
             become point 0 of the triangle. */
          localFaces[i].cornerPoints[0] = tmp[2];
          localFaces[i].cornerPoints[1] = tmp[0];
          localFaces[i].cornerPoints[2] = tmp[1];
        }
      }
      else if(cornerIsTopPyramid[1] && !cornerIsTopPyramid[2]) {

        /* cornerPoint[1] is a top of a pyramid and the other two corners
           are not. Change the sequence, such that cornerPoint[2] will become
           point 0 of the triangle. */
        unsigned long tmp[] = {localFaces[i].cornerPoints[0],
                               localFaces[i].cornerPoints[1],
                               localFaces[i].cornerPoints[2]};
        localFaces[i].cornerPoints[0] = tmp[2];
        localFaces[i].cornerPoints[1] = tmp[0];
        localFaces[i].cornerPoints[2] = tmp[1];
      }
    }
  }

  /*--- Determine the number of matching and non-matching internal faces. ---*/
  unsigned long nMatchingFaces = 0, nNonMatchingFaces = 0;
  for(unsigned long i=0; i<localFaces.size(); ++i) {
    if(localFaces[i].faceIndicator == -1) {
      if(localFaces[i].elemID1 < nVolElemTot) ++nMatchingFaces;
      else                                    ++nNonMatchingFaces;
    }
  }

  if( nNonMatchingFaces ) {
    cout << "CMeshFEM_DG::CreateFaces: "
         << nNonMatchingFaces << " non-matching internal faces found. "
         << "This is not supported yet." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*---------------------------------------------------------------------------*/
  /*--- Step 3: Create the local face based data structure for the internal ---*/
  /*---         faces. These are needed for the computation of the surface  ---*/
  /*---         integral in DG-FEM.                                         ---*/
  /*---------------------------------------------------------------------------*/

  /*--- Determine the sizes of the vectors, which store the connectivity of the faces.
        Note that these sizes must be determined beforehand, such that no resize needs
        to be carried out when the data is actually set. ---*/
  unsigned long sizeVecDOFsGridFaceSide0    = 0, sizeVecDOFsGridFaceSide1    = 0;
  unsigned long sizeVecDOFsSolFaceSide0     = 0, sizeVecDOFsSolFaceSide1     = 0;
  unsigned long sizeVecDOFsGridElementSide0 = 0, sizeVecDOFsGridElementSide1 = 0;
  unsigned long sizeVecDOFsSolElementSide0  = 0, sizeVecDOFsSolElementSide1  = 0;

  for(unsigned long i=0; i<localFaces.size(); ++i) {
    if(localFaces[i].faceIndicator == -1 && localFaces[i].elemID1 < nVolElemTot) {

      /* Determine from the face type and the polynomial degree used on both
         sides the number of DOFs for the grid and the solution. Update the
         corresponding counters accordingly. */
      switch( localFaces[i].nCornerPoints ) {
        case 2:
          /* Face is a line. */
          sizeVecDOFsGridFaceSide0 += localFaces[i].nPolyGrid0 + 1;
          sizeVecDOFsGridFaceSide1 += localFaces[i].nPolyGrid1 + 1;
          sizeVecDOFsSolFaceSide0  += localFaces[i].nPolySol0  + 1;
          sizeVecDOFsSolFaceSide1  += localFaces[i].nPolySol1  + 1;
          break;

        case 3:
          /* Face is a triangle. */
          sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+2)/2;
          sizeVecDOFsGridFaceSide1 += (localFaces[i].nPolyGrid1+1)*(localFaces[i].nPolyGrid1+2)/2;
          sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +2)/2;
          sizeVecDOFsSolFaceSide1  += (localFaces[i].nPolySol1 +1)*(localFaces[i].nPolySol1 +2)/2;
          break;

        case 4:
          /* Face is a quadrilateral. */
          sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+1);
          sizeVecDOFsGridFaceSide1 += (localFaces[i].nPolyGrid1+1)*(localFaces[i].nPolyGrid1+1);
          sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +1);
          sizeVecDOFsSolFaceSide1  += (localFaces[i].nPolySol1 +1)*(localFaces[i].nPolySol1 +1);
          break;
      }

      /* Update the sizes to store the DOFs of the adjacent elements. */
      unsigned long v0 = localFaces[i].elemID0;
      unsigned long v1 = localFaces[i].elemID1;

      sizeVecDOFsGridElementSide0 += volElem[v0].nDOFsGrid;
      sizeVecDOFsGridElementSide1 += volElem[v1].nDOFsGrid;
      sizeVecDOFsSolElementSide0  += volElem[v0].nDOFsSol;
      sizeVecDOFsSolElementSide1  += volElem[v1].nDOFsSol;
    }
  }

  /* Allocate the memory for the matching faces as well as the memory for the
     storage of the DOFs of the connectivities of the faces. */
  matchingFaces.resize(nMatchingFaces);

  VecDOFsGridFaceSide0.resize(sizeVecDOFsGridFaceSide0);
  VecDOFsGridFaceSide1.resize(sizeVecDOFsGridFaceSide1);
  VecDOFsSolFaceSide0.resize(sizeVecDOFsSolFaceSide0);
  VecDOFsSolFaceSide1.resize(sizeVecDOFsSolFaceSide1);

  VecDOFsGridElementSide0.resize(sizeVecDOFsGridElementSide0);
  VecDOFsGridElementSide1.resize(sizeVecDOFsGridElementSide1);
  VecDOFsSolElementSide0.resize(sizeVecDOFsSolElementSide0);
  VecDOFsSolElementSide1.resize(sizeVecDOFsSolElementSide1);

  /*--- Loop over the volume elements to determine the maximum number
        of DOFs for the volume elements. Allocate the memory for the
        vector used to store the DOFs of the element.  ---*/
  unsigned short nDOFsVolMax = 0;
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    nDOFsVolMax = max(nDOFsVolMax, volElem[i].nDOFsGrid);
    nDOFsVolMax = max(nDOFsVolMax, volElem[i].nDOFsSol);
  }

  vector<unsigned long> DOFsElem(nDOFsVolMax);

  /*--- Loop again over localFaces, but now store the required information
        in matchingFaces. ---*/
  sizeVecDOFsGridFaceSide0    = sizeVecDOFsGridFaceSide1    = 0;
  sizeVecDOFsSolFaceSide0     = sizeVecDOFsSolFaceSide1     = 0;
  sizeVecDOFsGridElementSide0 = sizeVecDOFsGridElementSide1 = 0;
  sizeVecDOFsSolElementSide0  = sizeVecDOFsSolElementSide1  = 0;

  unsigned long ii = 0;
  for(unsigned long i=0; i<localFaces.size(); ++i) {
    if(localFaces[i].faceIndicator == -1 && localFaces[i].elemID1 < nVolElemTot) {

      /* Set the pointers for the connectivities of the face. */
      matchingFaces[ii].DOFsGridFaceSide0 = VecDOFsGridFaceSide0.data() + sizeVecDOFsGridFaceSide0;
      matchingFaces[ii].DOFsGridFaceSide1 = VecDOFsGridFaceSide1.data() + sizeVecDOFsGridFaceSide1;
      matchingFaces[ii].DOFsSolFaceSide0  = VecDOFsSolFaceSide0.data()  + sizeVecDOFsSolFaceSide0;
      matchingFaces[ii].DOFsSolFaceSide1  = VecDOFsSolFaceSide1.data()  + sizeVecDOFsSolFaceSide1;

      matchingFaces[ii].DOFsGridElementSide0 = VecDOFsGridElementSide0.data() + sizeVecDOFsGridElementSide0;
      matchingFaces[ii].DOFsGridElementSide1 = VecDOFsGridElementSide1.data() + sizeVecDOFsGridElementSide1;
      matchingFaces[ii].DOFsSolElementSide0  = VecDOFsSolElementSide0.data()  + sizeVecDOFsSolElementSide0;
      matchingFaces[ii].DOFsSolElementSide1  = VecDOFsSolElementSide1.data()  + sizeVecDOFsSolElementSide1;

      /* Update the counters for the face connectivities. This depends on the
         type of surface element. */
      unsigned short VTK_Type;
      switch( localFaces[i].nCornerPoints ) {
        case 2:
          /* Face is a line. */
          VTK_Type = LINE;
          sizeVecDOFsGridFaceSide0 += localFaces[i].nPolyGrid0 + 1;
          sizeVecDOFsGridFaceSide1 += localFaces[i].nPolyGrid1 + 1;
          sizeVecDOFsSolFaceSide0  += localFaces[i].nPolySol0  + 1;
          sizeVecDOFsSolFaceSide1  += localFaces[i].nPolySol1  + 1;
          break;

        case 3:
          /* Face is a triangle. */
          VTK_Type = TRIANGLE;
          sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+2)/2;
          sizeVecDOFsGridFaceSide1 += (localFaces[i].nPolyGrid1+1)*(localFaces[i].nPolyGrid1+2)/2;
          sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +2)/2;
          sizeVecDOFsSolFaceSide1  += (localFaces[i].nPolySol1 +1)*(localFaces[i].nPolySol1 +2)/2;
          break;

        case 4:
          /* Face is a quadrilateral. */
          VTK_Type = QUADRILATERAL;
          sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+1);
          sizeVecDOFsGridFaceSide1 += (localFaces[i].nPolyGrid1+1)*(localFaces[i].nPolyGrid1+1);
          sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +1);
          sizeVecDOFsSolFaceSide1  += (localFaces[i].nPolySol1 +1)*(localFaces[i].nPolySol1 +1);
          break;
      }

      /* Update the counters for the adjacent element connectivities. */
      unsigned long v0 = localFaces[i].elemID0;
      unsigned long v1 = localFaces[i].elemID1;

      sizeVecDOFsGridElementSide0 += volElem[v0].nDOFsGrid;
      sizeVecDOFsGridElementSide1 += volElem[v1].nDOFsGrid;
      sizeVecDOFsSolElementSide0  += volElem[v0].nDOFsSol;
      sizeVecDOFsSolElementSide1  += volElem[v1].nDOFsSol;

      /*--- Create the connectivities of the adjacent elements in the correct
            sequence as well as the connectivities of the face. The
            connectivities of both sides of the face are determined for the
            grid and solution. First for side 0.   ---*/
      bool swapFaceInElementSide0;
      for(unsigned short j=0; j<volElem[v0].nDOFsSol; ++j)
        DOFsElem[j] = volElem[v0].offsetDOFsSolLocal + j;

      CreateConnectivitiesFace(VTK_Type,                localFaces[i].cornerPoints,
                               volElem[v0].VTK_Type,    volElem[v0].nPolyGrid,
                               volElem[v0].nodeIDsGrid, volElem[v0].nPolySol,
                               DOFsElem.data(),         swapFaceInElementSide0,
                               matchingFaces[ii].DOFsSolFaceSide0,
                               matchingFaces[ii].DOFsSolElementSide0);

      for(unsigned short j=0; j<volElem[v0].nDOFsGrid; ++j)
        DOFsElem[j] = volElem[v0].nodeIDsGrid[j];

      CreateConnectivitiesFace(VTK_Type,                localFaces[i].cornerPoints,
                               volElem[v0].VTK_Type,    volElem[v0].nPolyGrid,
                               volElem[v0].nodeIDsGrid, volElem[v0].nPolyGrid,
                               DOFsElem.data(),         swapFaceInElementSide0,
                               matchingFaces[ii].DOFsGridFaceSide0,
                               matchingFaces[ii].DOFsGridElementSide0);

      /*--- And also for side 1 of the face. ---*/
      bool swapFaceInElementSide1;
      for(unsigned short j=0; j<volElem[v1].nDOFsSol; ++j)
        DOFsElem[j] = volElem[v1].offsetDOFsSolLocal + j;

      CreateConnectivitiesFace(VTK_Type,                localFaces[i].cornerPoints,
                               volElem[v1].VTK_Type,    volElem[v1].nPolyGrid,
                               volElem[v1].nodeIDsGrid, volElem[v1].nPolySol,
                               DOFsElem.data(),         swapFaceInElementSide1,
                               matchingFaces[ii].DOFsSolFaceSide1,
                               matchingFaces[ii].DOFsSolElementSide1);

      for(unsigned short j=0; j<volElem[v1].nDOFsGrid; ++j)
        DOFsElem[j] = volElem[v1].nodeIDsGrid[j];

      CreateConnectivitiesFace(VTK_Type,                localFaces[i].cornerPoints,
                               volElem[v1].VTK_Type,    volElem[v1].nPolyGrid,
                               volElem[v1].nodeIDsGrid, volElem[v1].nPolyGrid,
                               DOFsElem.data(),         swapFaceInElementSide1,
                               matchingFaces[ii].DOFsGridFaceSide1,
                               matchingFaces[ii].DOFsGridElementSide1);

      /*--- Search in the standard elements for faces for a matching
            standard element. If not found, create a new standard element.
            Note that both the grid and the solution representation must
            match with the standard element. ---*/
      unsigned long j;
      for(j=0; j<standardMatchingFacesSol.size(); ++j) {
        if(standardMatchingFacesSol[j].SameStandardMatchingFace(VTK_Type,
                                                                localFaces[i].JacFaceIsConsideredConstant,
                                                                localFaces[i].elemType0,
                                                                localFaces[i].nPolySol0,
                                                                localFaces[i].elemType1,
                                                                localFaces[i].nPolySol1,
                                                                swapFaceInElementSide0,
                                                                swapFaceInElementSide1) &&
           standardMatchingFacesGrid[j].SameStandardMatchingFace(VTK_Type,
                                                                 localFaces[i].JacFaceIsConsideredConstant,
                                                                 localFaces[i].elemType0,
                                                                 localFaces[i].nPolyGrid0,
                                                                 localFaces[i].elemType1,
                                                                 localFaces[i].nPolyGrid1,
                                                                 swapFaceInElementSide0,
                                                                 swapFaceInElementSide1) ) {
          matchingFaces[ii].indStandardElement = j;
          break;
        }
      }

      /* Create the new standard elements if no match was found. */
      if(j == standardMatchingFacesSol.size()) {

        standardMatchingFacesSol.push_back(FEMStandardInternalFaceClass(VTK_Type,
                                                                        localFaces[i].elemType0,
                                                                        localFaces[i].nPolySol0,
                                                                        localFaces[i].elemType1,
                                                                        localFaces[i].nPolySol1,
                                                                        localFaces[i].JacFaceIsConsideredConstant,
                                                                        swapFaceInElementSide0,
                                                                        swapFaceInElementSide1,
                                                                        config) );

        standardMatchingFacesGrid.push_back(FEMStandardInternalFaceClass(VTK_Type,
                                                                         localFaces[i].elemType0,
                                                                         localFaces[i].nPolyGrid0,
                                                                         localFaces[i].elemType1,
                                                                         localFaces[i].nPolyGrid1,
                                                                         localFaces[i].JacFaceIsConsideredConstant,
                                                                         swapFaceInElementSide0,
                                                                         swapFaceInElementSide1,
                                                                         config,
                                                                         standardMatchingFacesSol[j].GetOrderExact()) );
        matchingFaces[ii].indStandardElement = j;
      }

      /* Update the counter ii for the next internal matching face. */
      ++ii;
    }
  }

  /*---------------------------------------------------------------------------*/
  /*--- Step 4: Create the local face based data structure for the faces    ---*/
  /*---         that belong to the physical boundaries.                     ---*/
  /*---------------------------------------------------------------------------*/

  /*--- Loop over the boundary markers. The periodic boundaries are skipped,
        because these are not physical boundaries and are treated via the
        halo elements, which are already in place. ---*/
  unsigned long indBegMarker = 0;
  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {
    if( !boundaries[iMarker].periodicBoundary ) {

      /* Determine the end index for this marker in localFaces. Note that
         localFaces is sorted such that the boundary faces are first and
         also grouped per boundary, see the functor SortFacesClass. */
      unsigned long indEndMarker = indBegMarker;
      for(; indEndMarker<localFaces.size(); ++indEndMarker) {
        if(localFaces[indEndMarker].faceIndicator != (short) iMarker) break;
      }

      /*--- Determine the sizes of the vectors, which store the connectivity of the faces.
            Note that these sizes must be determined beforehand, such that no resize needs
            to be carried out when the data is actually set. ---*/
      sizeVecDOFsGridFaceSide0    = sizeVecDOFsSolFaceSide0    = 0;
      sizeVecDOFsGridElementSide0 = sizeVecDOFsSolElementSide0 = 0;

      for(unsigned long i=indBegMarker; i<indEndMarker; ++i) {
        /* Determine from the face type and the polynomial degree used the number of DOFs for
           the grid and the solution. Update the corresponding counters accordingly. */
        switch( localFaces[i].nCornerPoints ) {
          case 2:
            /* Face is a line. */
            sizeVecDOFsGridFaceSide0 += localFaces[i].nPolyGrid0 + 1;
            sizeVecDOFsSolFaceSide0  += localFaces[i].nPolySol0  + 1;
            break;

          case 3:
            /* Face is a triangle. */
            sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+2)/2;
            sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +2)/2;
            break;

          case 4:
            /* Face is a quadrilateral. */
            sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+1);
            sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +1);
            break;
        }

        /* Update the sizes to store the DOFs of the adjacent elements. */
        unsigned long v0 = localFaces[i].elemID0;

        sizeVecDOFsGridElementSide0 += volElem[v0].nDOFsGrid;
        sizeVecDOFsSolElementSide0  += volElem[v0].nDOFsSol;
      }

      /* Allocate the memory for the storage of the DOFs of the
         connectivities of the faces. */
      boundaries[iMarker].VecDOFsGridFace.resize(sizeVecDOFsGridFaceSide0);
      boundaries[iMarker].VecDOFsSolFace.resize(sizeVecDOFsSolFaceSide0);
      boundaries[iMarker].VecDOFsGridElement.resize(sizeVecDOFsGridElementSide0);
      boundaries[iMarker].VecDOFsSolElement.resize(sizeVecDOFsSolElementSide0);

      /* Easier storage of the surface elements for this boundary. */
      vector<CSurfaceElementFEM> &surfElem = boundaries[iMarker].surfElem;

      /*--- Loop again over localFaces for this boundary, but now
            store the required information in surfElem. ---*/
      sizeVecDOFsGridFaceSide0    = sizeVecDOFsSolFaceSide0    = 0;
      sizeVecDOFsGridElementSide0 = sizeVecDOFsSolElementSide0 = 0;
      for(unsigned long i=indBegMarker; i<indEndMarker; ++i) {

        /* Set the pointers for the connectivities of the face. */
        ii = i - indBegMarker;
        surfElem[ii].DOFsGridFace = boundaries[iMarker].VecDOFsGridFace.data() + sizeVecDOFsGridFaceSide0;
        surfElem[ii].DOFsSolFace  = boundaries[iMarker].VecDOFsSolFace.data()  + sizeVecDOFsSolFaceSide0;

        surfElem[ii].DOFsGridElement = boundaries[iMarker].VecDOFsGridElement.data() + sizeVecDOFsGridElementSide0;
        surfElem[ii].DOFsSolElement  = boundaries[iMarker].VecDOFsSolElement.data()  + sizeVecDOFsSolElementSide0;

        /* Update the counters for the face connectivities. This depends on the
           type of surface element. */
        unsigned short VTK_Type;
        switch( localFaces[i].nCornerPoints ) {
          case 2:
            /* Face is a line. */
            VTK_Type = LINE;
            sizeVecDOFsGridFaceSide0 += localFaces[i].nPolyGrid0 + 1;
            sizeVecDOFsSolFaceSide0  += localFaces[i].nPolySol0  + 1;
            break;

          case 3:
            /* Face is a triangle. */
            VTK_Type = TRIANGLE;
            sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+2)/2;
            sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +2)/2;
            break;

          case 4:
            /* Face is a quadrilateral. */
            VTK_Type = QUADRILATERAL;
            sizeVecDOFsGridFaceSide0 += (localFaces[i].nPolyGrid0+1)*(localFaces[i].nPolyGrid0+1);
            sizeVecDOFsSolFaceSide0  += (localFaces[i].nPolySol0 +1)*(localFaces[i].nPolySol0 +1);
            break;
        }

        /* Update the counters for the adjacent element connectivities. */
        unsigned long v0 = localFaces[i].elemID0;

        sizeVecDOFsGridElementSide0 += volElem[v0].nDOFsGrid;
        sizeVecDOFsSolElementSide0  += volElem[v0].nDOFsSol;

        /*--- Create the connectivities of the adjacent element in the correct
              sequence as well as the connectivities of the face. The
              connectivities are determined for the grid and solution. ---*/
        bool swapFaceInElement;
        for(unsigned short j=0; j<volElem[v0].nDOFsSol; ++j)
          DOFsElem[j] = volElem[v0].offsetDOFsSolLocal + j;

        CreateConnectivitiesFace(VTK_Type,                 localFaces[i].cornerPoints,
                                 volElem[v0].VTK_Type,     volElem[v0].nPolyGrid,
                                 volElem[v0].nodeIDsGrid,  volElem[v0].nPolySol,
                                 DOFsElem.data(),          swapFaceInElement,
                                 surfElem[ii].DOFsSolFace, surfElem[ii].DOFsSolElement);

        for(unsigned short j=0; j<volElem[v0].nDOFsGrid; ++j)
          DOFsElem[j] = volElem[v0].nodeIDsGrid[j];

        CreateConnectivitiesFace(VTK_Type,                  localFaces[i].cornerPoints,
                                 volElem[v0].VTK_Type,      volElem[v0].nPolyGrid,
                                 volElem[v0].nodeIDsGrid,   volElem[v0].nPolyGrid,
                                 DOFsElem.data(),           swapFaceInElement,
                                 surfElem[ii].DOFsGridFace, surfElem[ii].DOFsGridElement);

        /*--- Search in the standard elements for boundary faces for a matching
              standard element. If not found, create a new standard element.
              Note that both the grid and the solution representation must
              match with the standard element. ---*/
        unsigned long j;
        for(j=0; j<standardBoundaryFacesSol.size(); ++j) {
          if(standardBoundaryFacesSol[j].SameStandardBoundaryFace(VTK_Type,
                                                                  localFaces[i].JacFaceIsConsideredConstant,
                                                                  localFaces[i].elemType0,
                                                                  localFaces[i].nPolySol0,
                                                                  swapFaceInElement) &&
             standardBoundaryFacesGrid[j].SameStandardBoundaryFace(VTK_Type,
                                                                   localFaces[i].JacFaceIsConsideredConstant,
                                                                   localFaces[i].elemType0,
                                                                   localFaces[i].nPolyGrid0,
                                                                   swapFaceInElement) ) {
            surfElem[ii].indStandardElement = j;
            break;
          }
        }

        /* Create the new standard elements if no match was found. */
        if(j == standardBoundaryFacesSol.size()) {

          standardBoundaryFacesSol.push_back(FEMStandardBoundaryFaceClass(VTK_Type,
                                                                          localFaces[i].elemType0,
                                                                          localFaces[i].nPolySol0,
                                                                          localFaces[i].JacFaceIsConsideredConstant,
                                                                          swapFaceInElement,
                                                                          config) );

          standardBoundaryFacesGrid.push_back(FEMStandardBoundaryFaceClass(VTK_Type,
                                                                           localFaces[i].elemType0,
                                                                           localFaces[i].nPolyGrid0,
                                                                           localFaces[i].JacFaceIsConsideredConstant,
                                                                           swapFaceInElement,
                                                                           config,
                                                                           standardBoundaryFacesSol[j].GetOrderExact()) );
          surfElem[ii].indStandardElement = j;
        }
      }

      /* Set indBegMarker to indEndMarker for the next marker. */
      indBegMarker = indEndMarker;
    }
  }
}

void CMeshFEM_DG::CreateStandardVolumeElements(CConfig *config) {

  /*--- Loop over the volume elements and create new standard elements if needed.
        Note that a new standard elements is created when either the solution
        element or the grid element does not match. Note further that for the
        standard element of the grid the exact order for the integration of the
        solution is used, such that the metric terms are computed in the correct
        integration points in case the polynomial order of the solution differs
        from that of the grid. ---*/
  for(unsigned long i=0; i<nVolElemTot; ++i) {

    if( volElem[i].elemIsOwned ) {

      /* Check the existing standard elements in the list. */
      unsigned long j;
      for(j=0; j<standardElementsSol.size(); ++j) {
        if(standardElementsSol[j].SameStandardElement(volElem[i].VTK_Type,
                                                      volElem[i].nPolySol,
                                                      volElem[i].JacIsConsideredConstant) &&
           standardElementsSol[j].SameStandardElement(volElem[i].VTK_Type,
                                                      volElem[i].nPolyGrid,
                                                      volElem[i].JacIsConsideredConstant) ) {
           volElem[i].indStandardElement = j;
           break;
        }
      }

      /* Create the new standard elements if no match was found. */
      if(j == standardElementsSol.size()) {

        standardElementsSol.push_back(FEMStandardElementClass(volElem[i].VTK_Type,
                                                              volElem[i].nPolySol,
                                                              volElem[i].JacIsConsideredConstant,
                                                              config) );

        standardElementsGrid.push_back(FEMStandardElementClass(volElem[i].VTK_Type,
                                                               volElem[i].nPolyGrid,
                                                               volElem[i].JacIsConsideredConstant,
                                                               config,
                                                               standardElementsSol[j].GetOrderExact()) );
        volElem[i].indStandardElement = j;
      }
    }
  }

}

void CMeshFEM_DG::SetSendReceive(CConfig *config) {

  /*--- Determine the number of ranks and the current rank. ---*/
  int nRank = SINGLE_NODE;

#ifdef HAVE_MPI
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nRank);
#endif

  /*----------------------------------------------------------------------------*/
  /*--- Step 1: Determine the ranks which this rank has to communicate       ---*/
  /*---         during the actual communication of halo data, as well as the ---*/
  /*            data that must be communicated.                              ---*/
  /*----------------------------------------------------------------------------*/

  /* Determine for every element the local offset of the solution DOFs. */
  volElem[0].offsetDOFsSolLocal = 0;
  for(unsigned long i=1; i<nVolElemTot; ++i)
    volElem[i].offsetDOFsSolLocal = volElem[i-1].offsetDOFsSolLocal
                                  + volElem[i-1].nDOFsSol;

  /* Determine the ranks which this rank will communicate. */
  vector<bool> commWithRank(nRank, false);
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    if( !volElem[i].elemIsOwned ) commWithRank[volElem[i].rankOriginal] = true;
  }

  map<int,int> rankToIndCommBuf;
  for(int i=0; i<nRank; ++i) {
    if( commWithRank[i] ) {
      int ind = rankToIndCommBuf.size();
      rankToIndCommBuf[i] = ind;
    }
  }

  ranksComm.resize(rankToIndCommBuf.size());
  map<int,int>::const_iterator MI = rankToIndCommBuf.begin();
  for(unsigned long i=0; i<rankToIndCommBuf.size(); ++i, ++MI)
    ranksComm[i] = MI->first;

  /* Define and determine the buffers to send the global indices of my halo
     elements to the appropriate ranks and the vectors which store the
     DOFs that I will receive from these ranks. */
  vector<vector<unsigned long> > longBuf(rankToIndCommBuf.size(), vector<unsigned long>(0));
  DOFsReceive.resize(rankToIndCommBuf.size());

  for(unsigned long i=0; i<nVolElemTot; ++i) {
    if( !volElem[i].elemIsOwned ) {
      MI = rankToIndCommBuf.find(volElem[i].rankOriginal);
      longBuf[MI->second].push_back(volElem[i].elemIDGlobal);

      for(unsigned long j=0; j<volElem[i].nDOFsSol; ++j)
        DOFsReceive[MI->second].push_back(volElem[i].offsetDOFsSolLocal+j);
    }
  }

  /* Determine the mapping from global element ID to local owned element ID. */
  map<unsigned long,unsigned long> globalElemIDToLocalInd;
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    if( volElem[i].elemIsOwned )
      globalElemIDToLocalInd[volElem[i].elemIDGlobal] = i;
  }

  /* Resize the first index of the vectors to store the DOFs that must be sent. */
  DOFsSend.resize(rankToIndCommBuf.size());

  /*--- Make a distinction between sequential and parallel mode to
        determine the DOFs to be sent.                 ---*/

#ifdef HAVE_MPI
  /*--- Parallel mode. Send all the data using non-blocking sends. ---*/
  vector<MPI_Request> commReqs(ranksComm.size());

  for(unsigned long i=0; i<ranksComm.size(); ++i) {
    int dest = ranksComm[i];
    SU2_MPI::Isend(longBuf[i].data(), longBuf[i].size(), MPI_UNSIGNED_LONG,
                   dest, dest, MPI_COMM_WORLD, &commReqs[i]);
  }

  /* Loop over the number of ranks from which I receive data about the
     global element ID's that I must sent. */
  for(unsigned long i=0; i<ranksComm.size(); ++i) {

    /* Receive the messages in the order specified in ranksComm.
       First probe the message to find out the size. */
    MPI_Status status;
    MPI_Probe(ranksComm[i], rank, MPI_COMM_WORLD, &status);

    int sizeMess;
    MPI_Get_count(&status, MPI_UNSIGNED_LONG, &sizeMess);

    /* Allocate the memory for a buffer to receive the data
       and receive the data using a blocking receive. */
    vector<unsigned long> longRecvBuf(sizeMess);
    SU2_MPI::Recv(longRecvBuf.data(), sizeMess, MPI_UNSIGNED_LONG,
                  ranksComm[i], rank, MPI_COMM_WORLD, &status);

    /* Loop over the elements of longRecvBuf and set the contents
       of DOFsSend accordingly. */
    for(int j=0; j<sizeMess; ++j) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalElemIDToLocalInd.find(longRecvBuf[j]);

      if(LMI == globalElemIDToLocalInd.end()) {
        cout << "This should not happen in CMeshFEM_DG::SetSendReceive" << endl;
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
      }

      for(unsigned long k=0; k<volElem[LMI->second].nDOFsSol; ++k)
        DOFsSend[i].push_back(volElem[LMI->second].offsetDOFsSolLocal+k);
    }
  }

  /* Complete the non-blocking sends. */
  SU2_MPI::Waitall(ranksComm.size(), commReqs.data(), MPI_STATUSES_IGNORE);

#else
  /* Sequential mode. Search for the local index of the global element ID.
     Note that in sequential mode there are only halo elements when periodic
     boundaries are present in the grid. */
  if( longBuf.size() ) {
    for(unsigned long i=0; i<longBuf[0].size(); ++i) {
      map<unsigned long,unsigned long>::const_iterator LMI;
      LMI = globalElemIDToLocalInd.find(longBuf[0][i]);

      if(LMI == globalElemIDToLocalInd.end()) {
        cout << "This should not happen in CMeshFEM_DG::SetSendReceive" << endl;
        exit(EXIT_FAILURE);
      }

      for(unsigned long j=0; j<volElem[LMI->second].nDOFsSol; ++j)
        DOFsSend[0].push_back(volElem[LMI->second].offsetDOFsSolLocal+j);
    }
  }

#endif

  /*----------------------------------------------------------------------------*/
  /*--- Step 2: Determine the rotational periodic transformations as well as ---*/
  /*---         the halo elements for which these must be applied.           ---*/
  /*----------------------------------------------------------------------------*/

  /*--- Loop over the markers and determine the mapping for the rotationally
        periodic transformations. The mapping is from the marker to the first
        index in the vectors to store the rotationally periodic halo elements. ---*/
  map<short,unsigned short> mapRotationalPeriodicToInd;

  for(unsigned short iMarker=0; iMarker<nMarker; ++iMarker) {
    if(config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY) {

      su2double *angles = config->GetPeriodicRotAngles(config->GetMarker_All_TagBound(iMarker));
      if(fabs(angles[0]) > 1.e-5 || fabs(angles[1]) > 1.e-5 || fabs(angles[2]) > 1.e-5) {

        unsigned short curSize = mapRotationalPeriodicToInd.size();
        mapRotationalPeriodicToInd[iMarker] = curSize;
      }
    }
  }

  /* Store the rotationally periodic indices in rotPerMarkers. */
  rotPerMarkers.reserve(mapRotationalPeriodicToInd.size());
  for(map<short,unsigned short>::iterator SMI =mapRotationalPeriodicToInd.begin();
                                          SMI!=mapRotationalPeriodicToInd.end(); ++SMI)
    rotPerMarkers.push_back(SMI->first);

  /* Resize the first index of rotPerHalos to the correct size. */
  rotPerHalos.resize(mapRotationalPeriodicToInd.size());

  /*--- Loop over the volume elements and store the indices of the rotationally
        periodic halo elements in rotPerHalos.     ---*/
  for(unsigned long i=0; i<nVolElemTot; ++i) {
    if(volElem[i].periodIndexToDonor > -1) {
      map<short,unsigned short>::const_iterator SMI;
      SMI = mapRotationalPeriodicToInd.find(volElem[i].periodIndexToDonor);

      if(SMI != mapRotationalPeriodicToInd.end())
        rotPerHalos[SMI->second].push_back(i);
    }
  }
}

void CMeshFEM_DG::CreateConnectivitiesFace(
                                const unsigned short        VTK_TypeFace,
                                const unsigned long         *cornerPointsFace,
                                const unsigned short        VTK_TypeElem,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &elemNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connElem,
                                bool                        &swapFaceInElement,
                                unsigned long               *modConnFace,
                                unsigned long               *modConnElem) {

  /*--- Set swapFaceInElement to false. This variable is only relevant for
        triangular faces of a pyramid and quadrilateral faces of a prism.
        Only for these situations this variable will be passed to the
        appropriate functions. ---*/
  swapFaceInElement = false;

  /*--- Make a distinction between the types of the volume element and call
        the appropriate function to do the actual job. ---*/
  switch( VTK_TypeElem ) {
    case TRIANGLE:
      CreateConnectivitiesLineAdjacentTriangle(cornerPointsFace, nPolyGrid,
                                               elemNodeIDsGrid,  nPolyConn,
                                               connElem,         modConnFace,
                                               modConnElem);
      break;

    case QUADRILATERAL:
      CreateConnectivitiesLineAdjacentQuadrilateral(cornerPointsFace, nPolyGrid,
                                                    elemNodeIDsGrid,  nPolyConn,
                                                    connElem,         modConnFace,
                                                    modConnElem);
      break;

    case TETRAHEDRON:
      CreateConnectivitiesTriangleAdjacentTetrahedron(cornerPointsFace, nPolyGrid,
                                                      elemNodeIDsGrid,  nPolyConn,
                                                      connElem,         modConnFace,
                                                      modConnElem);
      break;

    case PYRAMID: {
      switch( VTK_TypeFace ) {
        case TRIANGLE:
          CreateConnectivitiesTriangleAdjacentPyramid(cornerPointsFace, nPolyGrid,
                                                      elemNodeIDsGrid,  nPolyConn,
                                                      connElem,         swapFaceInElement,
                                                      modConnFace,      modConnElem);
          break;

        case QUADRILATERAL:
          CreateConnectivitiesQuadrilateralAdjacentPyramid(cornerPointsFace, nPolyGrid,
                                                           elemNodeIDsGrid,  nPolyConn,
                                                           connElem,         modConnFace,
                                                           modConnElem);
          break;
      }

      break;
    }

    case PRISM: {
      switch( VTK_TypeFace ) {
        case TRIANGLE:
          CreateConnectivitiesTriangleAdjacentPrism(cornerPointsFace, nPolyGrid,
                                                    elemNodeIDsGrid,  nPolyConn,
                                                    connElem,         modConnFace,
                                                    modConnElem);
          break;

        case QUADRILATERAL:
          CreateConnectivitiesQuadrilateralAdjacentPrism(cornerPointsFace, nPolyGrid,
                                                         elemNodeIDsGrid,  nPolyConn,
                                                         connElem,         swapFaceInElement,
                                                         modConnFace,      modConnElem);
          break;
      }

      break;
    }

    case HEXAHEDRON:
      CreateConnectivitiesQuadrilateralAdjacentHexahedron(cornerPointsFace, nPolyGrid,
                                                          elemNodeIDsGrid,  nPolyConn,
                                                          connElem,         modConnFace,
                                                          modConnElem);
      break;
  }
}

void CMeshFEM_DG::CreateConnectivitiesLineAdjacentQuadrilateral(
                                const unsigned long         *cornerPointsLine,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &quadNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connQuad,
                                unsigned long               *modConnLine,
                                unsigned long               *modConnQuad) {

  /* Determine the indices of the four corner points of the quadrilateral. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+1) -1;
  const unsigned short ind3 = ind2 - nPolyGrid;

  /* Easier storage of the two corner points of the line in the new numbering. */
  const unsigned long vert0 = cornerPointsLine[0];
  const unsigned long vert1 = cornerPointsLine[1];

  /*--- There exists a linear mapping from the indices of the numbering used in
        quadNodeIDsGrid to the indices of the target numbering. This mapping is of
        the form ii = a + b*i + c*j and jj = d + e*i + f*j, where ii,jj are the
        indices of the new numbering and i,j the indices of the numbering used
        in quadNodeIDsGrid (and connQuad). The values of the coefficients a,b,c,d,e,f
        depend on how the corner points of the line coincide with the corner points
        of the quad. This is determined below. The bool verticesDontMatch is
        there to check if vertices do not match. This should not happen, but
        it is checked for security. ---*/
  signed short a, b, c, d, e, f;
  bool verticesDontMatch = false;

  if(vert0 == quadNodeIDsGrid[ind0]) {
    /* Vert0 coincides with vertex 0 of the quad connectivity.
       Determine the situation for vert1. */
    if(vert1 == quadNodeIDsGrid[ind1]){
      /* The new numbering is the same as the original numbering. */
      a = d = 0; b = f = 1; c = e = 0;
    }
    else if(vert1 == quadNodeIDsGrid[ind3]) {
      /* The i and j numbering are swapped. This is a left handed transformation. */
      a = d = 0; b = f = 0; c = e = 1;
    }
    else {
      verticesDontMatch = true;  // Vert1 does not match with a neigbor.
    }
  }
  else if(vert0 == quadNodeIDsGrid[ind1]) {
    /* Vert0 coincides with vertex 1 of the quad connectivity.
       Determine the situation for vert1. */
    if(vert1 == quadNodeIDsGrid[ind2]){
      /* The i-direction of the new numbering corresponds to the j-direction
         of the original numbering, while the new j-direction is the negative
         i-direction of the original numbering. */
      a = 0; d = nPolyConn; b = f = 0; c = 1; e = -1;
    }
    else if(vert1 == quadNodeIDsGrid[ind0]) {
      /* The i-direction is negated, while the j-direction coincides.
         This is a left handed transformation. */
      a = nPolyConn; d = 0; b = -1; f = 1; c = e = 0;
    }
    else {
      verticesDontMatch = true;  // Vert1 does not match with a neigbor.
    }
  }
  else if(vert0 == quadNodeIDsGrid[ind2]) {
    /* Vert0 coincides with vertex 2 of the quad connectivity.
       Determine the situation for vert1. */
    if(vert1 == quadNodeIDsGrid[ind3]){
      /* Both the i- and j-direction are negated. */
      a = d = nPolyConn; b = f = -1; c = e = 0;
    }
    else if(vert1 == quadNodeIDsGrid[ind1]) {
      /* The new i-direction is the original negative j-direction, while the
         new j-direction is the original negative i-direction. This is a
         left handed transformation. */
      a = d = nPolyConn; b = f = 0; c = e = -1;
    }
    else {
      verticesDontMatch = true;  // Vert1 does not match with a neigbor.
    }
  }
  else if(vert0 == quadNodeIDsGrid[ind3]) {
    /* Vert0 coincides with vertex 3 of the quad connectivity.
       Determine the situation for vert1. */
    if(vert1 == quadNodeIDsGrid[ind0]){
      /* The new i-direction is the original negative j-direction, while the
         new j-direction is the original i-direction. */
      a = nPolyConn; d = 0; b = f = 0; c = -1; e = 1;
    }
    else if(vert1 == quadNodeIDsGrid[ind2]){
      /* The i-directions coincide while the j-direction is negated.
         This is a left handed transformation. */
      a = 0; d = nPolyConn; b = 1; f = -1; c = e = 0;
    }
    else {
      verticesDontMatch = true;  // Vert1 does not match with a neigbor.
    }
  }
  else {
    /* Vert0 does not match with any of the corner vertices of the quad. */
    verticesDontMatch = true;
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesLineAdjacentQuadrilateral." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Loop over the DOFs of the original quad to create the connectivity
        of the quad that corresponds to the new numbering. ---*/
  unsigned short ind = 0;
  for(unsigned short j=0; j<=nPolyConn; ++j) {
    for(unsigned short i=0; i<=nPolyConn; ++i, ++ind) {

      /*--- Determine the ii and jj indices of the new numbering, convert it to
            a 1D index and shore the modified index in modConnQuad. ---*/
      unsigned short ii   = a + i*b + j*c;
      unsigned short jj   = d + i*e + j*f;
      unsigned short iind = jj*(nPolyConn+1) + ii;

      modConnQuad[iind] = connQuad[ind];
    }
  }

  /*--- The line corresponds to face 0 of the quadrilateral. Hence the first
        nPolyConn+1 entries in modConnQuad are the DOFs of the line. Copy
        these entries from modConnQuad. ---*/
  for(unsigned short i=0; i<=nPolyConn; ++i)
    modConnLine[i] = modConnQuad[i];
}

void CMeshFEM_DG::CreateConnectivitiesLineAdjacentTriangle(
                                const unsigned long         *cornerPointsLine,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &triaNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connTria,
                                unsigned long               *modConnLine,
                                unsigned long               *modConnTria) {

  /* Determine the indices of the 3 corner vertices of the triangle. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+2)/2 -1;

  /* Easier storage of the two corner points of the line in the new numbering. */
  const unsigned long vert0 = cornerPointsLine[0];
  const unsigned long vert1 = cornerPointsLine[1];

  /*--- There exists a linear mapping from the indices of the numbering used in
        triaNodeIDsGrid to the indices of the target numbering. This mapping is of
        the form ii = a + b*i + c*j and jj = d + e*i + f*j, where ii,jj are the
        indices of the new numbering and i,j the indices of the numbering used
        in triaNodeIDsGrid (and connTria). The values of the coefficients a,b,c,d,e,f
        depend on how the corner points of the line coincide with the corner points
        of the triangle. This is determined below. The bool verticesDontMatch is
        there to check if vertices do not match. This should not happen, but
        it is checked for security. ---*/
  signed short a, b, c, d, e, f;
  bool verticesDontMatch = false;

  if(vert0 == triaNodeIDsGrid[ind0]) {
    /* Vert0 coincides with vertex 0 of the triangle connectivity.
       Determine the situation for vert1. */
    if(vert1 == triaNodeIDsGrid[ind1]){
      /* The new numbering is the same as the original numbering. */
      a = 0; b = 1; c = 0; d = 0; e = 0; f = 1;
    }
    else if(vert1 == triaNodeIDsGrid[ind2]) {
      /* The i and j numbering are swapped. This is a left handed transformation. */
      a = 0; b = 0; c = 1; d = 0; e = 1; f = 0;
    }
    else {
      verticesDontMatch = true;  // Vert1 does not match with a neigbor.
    }
  }
  else if(vert0 == triaNodeIDsGrid[ind1]) {
    /* Vert0 coincides with vertex 1 of the triangle connectivity.
       Determine the situation for vert1. */
    if(vert1 == triaNodeIDsGrid[ind2]){
      /* The i-direction of the new numbering corresponds to the j-direction
         of the original numbering, while the new j-direction corresponds to
         a combination of the original i- and j-direction. */
      a = 0; b = 0; c = 1; d = nPolyConn; e = -1; f = -1;
    }
    else if(vert1 == triaNodeIDsGrid[ind0]) {
      /* The i-direction of the new numbering corresponds to a combination of
         the original i- and j-direction, while the new j-direction corresponds
         to the j-direction of the original numbering. This is a left
         handed transformation. */
      a = nPolyConn; b = -1; c = -1; d = 0; e = 0; f = 1;
    }
    else {
      verticesDontMatch = true;  // Vert1 does not match with a neigbor.
    }
  }
  else if(vert0 == triaNodeIDsGrid[ind2]) {
    /* Vert0 coincides with vertex 2 of the triangle connectivity.
       Determine the situation for vert1. */
    if(vert1 == triaNodeIDsGrid[ind0]){
      /* The i-direction of the new numbering corresponds to a combination of
         the original i- and j-direction, while the new j-direction corresponds
         to the i-direction of the original numbering. */
      a = nPolyConn; b = -1; c = -1; d = 0; e = 1; f = 0;
    }
    else if(vert1 == triaNodeIDsGrid[ind1]) {
      /* The i-direction of the new numbering corresponds to the i-direction of
         the original numbering, while the new j-direction corresponds to a
         combination of the original i- and j-direction. This is a left handed
         transformation. */
      a = 0; b = 1; c = 0; d = nPolyConn; e = -1; f = -1;
    }
    else {
      verticesDontMatch = true;  // Vert1 does not match with a neigbor.
    }
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesLineAdjacentTriangle." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Loop over the DOFs of the original triangle to create the connectivity
        of the triangle that corresponds to the new numbering. ---*/
  unsigned short ind = 0;
  for(unsigned short j=0; j<=nPolyConn; ++j) {
    for(unsigned short i=0; i<=(nPolyConn-j); ++i, ++ind) {

      /*--- Determine the ii and jj indices of the new numbering, convert it to
            a 1D index and shore the modified index in modConnTria. ---*/
      unsigned short ii = a + i*b + j*c;
      unsigned short jj = d + i*e + j*f;

      unsigned short iind = jj*(nPolyConn+1) + ii - jj*(jj-1)/2;

      modConnTria[iind] = connTria[ind];
    }
  }

  /*--- The line corresponds to face 0 of the triangle. Hence the first
        nPolyConn+1 entries in modConnTria are the DOFs of the line. Copy
        these entries from modConnTria. ---*/
  for(unsigned short i=0; i<=nPolyConn; ++i)
    modConnLine[i] = modConnTria[i];
}

void CMeshFEM_DG::CreateConnectivitiesQuadrilateralAdjacentHexahedron(
                                const unsigned long         *cornerPointsQuad,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &hexaNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connHexa,
                                unsigned long               *modConnQuad,
                                unsigned long               *modConnHexa) {

  /* Determine the indices of the eight corner points of the hexahedron. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+1) -1;
  const unsigned short ind3 = ind2 - nPolyGrid;
  const unsigned short ind4 = (nPolyGrid+1)*(nPolyGrid+1)*nPolyGrid;
  const unsigned short ind5 = ind1 + ind4;
  const unsigned short ind6 = ind2 + ind4;
  const unsigned short ind7 = ind3 + ind4;

  /* Easier storage of the four corner points of the quad in the new numbering. */
  const unsigned long vert0 = cornerPointsQuad[0];
  const unsigned long vert1 = cornerPointsQuad[1];
  const unsigned long vert2 = cornerPointsQuad[2];
  const unsigned long vert3 = cornerPointsQuad[3];

  /*--- There exists a linear mapping from the indices of the numbering used in
        hexaNodeIDsGrid to the indices of the target numbering. This mapping is of
        the form ii = a + b*i + c*j + d*k, jj = e + f*i + g*j + h*k and
        kk = l + m*i + n*j + o*k, where ii,jj,kk are the indices of the new
        numbering and i,j,k the indices of the numbering used in in hexaNodeIDsGrid
        (and connHexa). The values of the coefficients a,b,c,d,e,f,g,h,l,m,n,o
        depend on how the corner points of the quad coincide with the corner points
        of the hexahedron. This is determined below. The bool verticesDontMatch is
        there to check if vertices do not match. This should not happen, but
        it is checked for security. ---*/
  signed short a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0,
               l = 0, m = 0, n = 0, o = 0;
  bool verticesDontMatch = false;

  if(vert0 == hexaNodeIDsGrid[ind0] && vert1 == hexaNodeIDsGrid[ind1] &&   // ii = i.
     vert2 == hexaNodeIDsGrid[ind2] && vert3 == hexaNodeIDsGrid[ind3]) {   // jj = j.
    b = g = o = 1;                                                         // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind0] && vert1 == hexaNodeIDsGrid[ind3] &&   // ii = j.
          vert2 == hexaNodeIDsGrid[ind2] && vert3 == hexaNodeIDsGrid[ind1]) {   // jj = i.
    c = f = o = 1;                                                              // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind0] && vert1 == hexaNodeIDsGrid[ind1] && // ii = i.
          vert2 == hexaNodeIDsGrid[ind5] && vert3 == hexaNodeIDsGrid[ind4]) { // jj = k.
    b = h = n = 1;                                                            // kk = j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind0] && vert1 == hexaNodeIDsGrid[ind4] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind5] && vert3 == hexaNodeIDsGrid[ind1]) { // jj = i.
    d = f = n = 1;                                                            // kk = j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind0] && vert1 == hexaNodeIDsGrid[ind3] && // ii = j.
          vert2 == hexaNodeIDsGrid[ind7] && vert3 == hexaNodeIDsGrid[ind4]) { // jj = k.
    c = h = m = 1;                                                            // kk = i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind0] && vert1 == hexaNodeIDsGrid[ind4] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind7] && vert3 == hexaNodeIDsGrid[ind3]) { // jj = j.
    d = g = m = 1;                                                            // kk = i.
  }

  else if(vert0 == hexaNodeIDsGrid[ind1] && vert1 == hexaNodeIDsGrid[ind0] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind3] && vert3 == hexaNodeIDsGrid[ind2]) { // jj = j.
    a = nPolyConn; b = -1; g = o = 1;                                         // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind1] && vert1 == hexaNodeIDsGrid[ind2] && // ii = j.
          vert2 == hexaNodeIDsGrid[ind3] && vert3 == hexaNodeIDsGrid[ind0]) { // jj = nPoly-i.
    e = nPolyConn; f = -1; c = o = 1;                                         // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind1] && vert1 == hexaNodeIDsGrid[ind0] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind4] && vert3 == hexaNodeIDsGrid[ind5]) { // jj = k.
    a = nPolyConn; b = -1; h = n = 1;                                         // kk = j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind1] && vert1 == hexaNodeIDsGrid[ind5] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind4] && vert3 == hexaNodeIDsGrid[ind0]) { // jj = nPoly-i.
    e = nPolyConn; f = -1; d = n = 1;                                         // kk = j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind1] && vert1 == hexaNodeIDsGrid[ind2] && // ii = j.
          vert2 == hexaNodeIDsGrid[ind6] && vert3 == hexaNodeIDsGrid[ind5]) { // jj = k.
    l = nPolyConn; m = -1; c = h = 1;                                         // kk = nPoly-i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind1] && vert1 == hexaNodeIDsGrid[ind5] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind6] && vert3 == hexaNodeIDsGrid[ind2]) { // jj = j.
    l = nPolyConn; m = -1; d = g = 1;                                         // kk = nPoly-i.
  }

  else if(vert0 == hexaNodeIDsGrid[ind2] && vert1 == hexaNodeIDsGrid[ind1] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind0] && vert3 == hexaNodeIDsGrid[ind3]) { // jj = nPoly-i.
    a = e = nPolyConn; c = f = -1; o = 1;                                     // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind2] && vert1 == hexaNodeIDsGrid[ind3] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind0] && vert3 == hexaNodeIDsGrid[ind1]) { // jj = nPoly-j.
    a = e = nPolyConn; b = g = -1; o = 1;                                     // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind2] && vert1 == hexaNodeIDsGrid[ind1] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind5] && vert3 == hexaNodeIDsGrid[ind6]) { // jj = k.
    a = l = nPolyConn; c = m = -1; h = 1;                                     // kk = nPoly-i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind2] && vert1 == hexaNodeIDsGrid[ind6] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind5] && vert3 == hexaNodeIDsGrid[ind1]) { // jj = nPoly-j.
    e = l = nPolyConn; g = m = -1; d = 1;                                     // kk = nPoly-i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind2] && vert1 == hexaNodeIDsGrid[ind3] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind7] && vert3 == hexaNodeIDsGrid[ind6]) { // jj = k.
    a = l = nPolyConn; b = n = -1; h = 1;                                     // kk = nPoly-j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind2] && vert1 == hexaNodeIDsGrid[ind6] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind7] && vert3 == hexaNodeIDsGrid[ind3]) { // jj = nPoly-i.
    e = l = nPolyConn; f = n = -1; d = 1;                                     // kk = nPoly-j.
  }

  else if(vert0 == hexaNodeIDsGrid[ind3] && vert1 == hexaNodeIDsGrid[ind0] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind1] && vert3 == hexaNodeIDsGrid[ind2]) { // jj = i.
    a = nPolyConn; c = -1; f = o = 1;                                         // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind3] && vert1 == hexaNodeIDsGrid[ind2] && // ii = i.
          vert2 == hexaNodeIDsGrid[ind1] && vert3 == hexaNodeIDsGrid[ind0]) { // jj = nPoly-j.
    e = nPolyConn; g = -1; b = o = 1;                                         // kk = k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind3] && vert1 == hexaNodeIDsGrid[ind0] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind4] && vert3 == hexaNodeIDsGrid[ind7]) { // jj = k.
    a = nPolyConn; c = -1; h = m = 1;                                         // kk = i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind3] && vert1 == hexaNodeIDsGrid[ind7] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind4] && vert3 == hexaNodeIDsGrid[ind0]) { // jj = nPoly-j.
    e = nPolyConn; g = -1; d = m = 1;                                         // kk = i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind3] && vert1 == hexaNodeIDsGrid[ind2] && // ii = i.
          vert2 == hexaNodeIDsGrid[ind6] && vert3 == hexaNodeIDsGrid[ind7]) { // jj = k.
    l = nPolyConn; n = -1; b = h = 1;                                         // kk = nPoly-j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind3] && vert1 == hexaNodeIDsGrid[ind7] && // ii = k.
          vert2 == hexaNodeIDsGrid[ind6] && vert3 == hexaNodeIDsGrid[ind2]) { // jj = i.
    l = nPolyConn; n = -1; d = f = 1;                                         // kk = nPoly-j.
  }

  else if(vert0 == hexaNodeIDsGrid[ind4] && vert1 == hexaNodeIDsGrid[ind5] && // ii = i.
          vert2 == hexaNodeIDsGrid[ind6] && vert3 == hexaNodeIDsGrid[ind7]) { // jj = j.
    l = nPolyConn; o = -1; b = g = 1;                                         // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind4] && vert1 == hexaNodeIDsGrid[ind7] && // ii = j.
          vert2 == hexaNodeIDsGrid[ind6] && vert3 == hexaNodeIDsGrid[ind5]) { // jj = i.
    l = nPolyConn; o = -1; c = f = 1;                                         // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind4] && vert1 == hexaNodeIDsGrid[ind5] && // ii = i.
          vert2 == hexaNodeIDsGrid[ind1] && vert3 == hexaNodeIDsGrid[ind0]) { // jj = nPoly-k.
    e = nPolyConn; h = -1; b = n = 1;                                         // kk = j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind4] && vert1 == hexaNodeIDsGrid[ind0] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind1] && vert3 == hexaNodeIDsGrid[ind5]) { // jj = i.
    a = nPolyConn; d = -1; f = n = 1;                                         // kk = j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind4] && vert1 == hexaNodeIDsGrid[ind7] && // ii = j.
          vert2 == hexaNodeIDsGrid[ind3] && vert3 == hexaNodeIDsGrid[ind0]) { // jj = nPoly-k.
    e = nPolyConn; h = -1; c = m = 1;                                         // kk = i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind4] && vert1 == hexaNodeIDsGrid[ind0] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind3] && vert3 == hexaNodeIDsGrid[ind7]) { // jj = j.
    a = nPolyConn; d = -1; g = m = 1;                                         // kk = i.
  }

  else if(vert0 == hexaNodeIDsGrid[ind5] && vert1 == hexaNodeIDsGrid[ind6] && // ii = j.
          vert2 == hexaNodeIDsGrid[ind7] && vert3 == hexaNodeIDsGrid[ind4]) { // jj = nPoly-i.
    e = l = nPolyConn; f = o = -1; c = 1;                                     // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind5] && vert1 == hexaNodeIDsGrid[ind4] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind7] && vert3 == hexaNodeIDsGrid[ind6]) { // jj = j.
    a = l = nPolyConn; b = o = -1; g = 1;                                     // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind5] && vert1 == hexaNodeIDsGrid[ind6] && // ii = j.
          vert2 == hexaNodeIDsGrid[ind2] && vert3 == hexaNodeIDsGrid[ind1]) { // jj = nPoly-k.
    e = l = nPolyConn; h = m = -1; c = 1;                                     // kk = nPoly-i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind5] && vert1 == hexaNodeIDsGrid[ind1] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind2] && vert3 == hexaNodeIDsGrid[ind6]) { // jj = j.
    a = l = nPolyConn; d = m = -1; g = 1;                                     // kk = nPoly-i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind5] && vert1 == hexaNodeIDsGrid[ind1] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind0] && vert3 == hexaNodeIDsGrid[ind4]) { // jj = nPoly-i.
    a = e = nPolyConn; d = f = -1; n = 1;                                     // kk = j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind5] && vert1 == hexaNodeIDsGrid[ind4] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind0] && vert3 == hexaNodeIDsGrid[ind1]) { // jj = nPoly-k.
    a = e = nPolyConn; b = h = -1; n = 1;                                     // kk = j.
  }

  else if(vert0 == hexaNodeIDsGrid[ind6] && vert1 == hexaNodeIDsGrid[ind7] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind4] && vert3 == hexaNodeIDsGrid[ind5]) { // jj = nPoly-j.
    a = e = l = nPolyConn; b = g = o = -1;                                    // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind6] && vert1 == hexaNodeIDsGrid[ind5] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind4] && vert3 == hexaNodeIDsGrid[ind7]) { // jj = nPoly-i.
    a = e = l = nPolyConn; c = f = o = -1;                                    // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind6] && vert1 == hexaNodeIDsGrid[ind7] && // ii = nPoly-i.
          vert2 == hexaNodeIDsGrid[ind3] && vert3 == hexaNodeIDsGrid[ind2]) { // jj = nPoly-k.
    a = e = l = nPolyConn; b = h = n = -1;                                    // kk = nPoly-j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind6] && vert1 == hexaNodeIDsGrid[ind2] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind3] && vert3 == hexaNodeIDsGrid[ind7]) { // jj = nPoly-i.
    a = e = l = nPolyConn; d = f = n = -1;                                    // kk = nPoly-j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind6] && vert1 == hexaNodeIDsGrid[ind2] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind1] && vert3 == hexaNodeIDsGrid[ind5]) { // jj = nPoly-j.
    a = e = l = nPolyConn; d = g = m = -1;                                    // kk = nPoly-i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind6] && vert1 == hexaNodeIDsGrid[ind5] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind1] && vert3 == hexaNodeIDsGrid[ind2]) { // jj = nPoly-k.
    a = e = l = nPolyConn; c = h = m = -1;                                    // kk = nPoly-i.
  }

  else if(vert0 == hexaNodeIDsGrid[ind7] && vert1 == hexaNodeIDsGrid[ind4] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind5] && vert3 == hexaNodeIDsGrid[ind6]) { // jj = i.
    a = l = nPolyConn; c = o = -1; f = 1;                                     // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind7] && vert1 == hexaNodeIDsGrid[ind6] && // ii = i.
          vert2 == hexaNodeIDsGrid[ind5] && vert3 == hexaNodeIDsGrid[ind4]) { // jj = nPoly-j.
    e = l = nPolyConn; g = o = -1; b = 1;                                     // kk = nPoly-k.
  }
  else if(vert0 == hexaNodeIDsGrid[ind7] && vert1 == hexaNodeIDsGrid[ind4] && // ii = nPoly-j.
          vert2 == hexaNodeIDsGrid[ind0] && vert3 == hexaNodeIDsGrid[ind3]) { // jj = nPoly-k.
    a = e = nPolyConn; c = h = -1; m = 1;                                     // kk = i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind7] && vert1 == hexaNodeIDsGrid[ind3] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind0] && vert3 == hexaNodeIDsGrid[ind4]) { // jj = nPoly-j.
    a = e = nPolyConn; d = g = -1; m = 1;                                     // kk = i.
  }
  else if(vert0 == hexaNodeIDsGrid[ind7] && vert1 == hexaNodeIDsGrid[ind6] && // ii = i.
          vert2 == hexaNodeIDsGrid[ind2] && vert3 == hexaNodeIDsGrid[ind3]) { // jj = nPoly-k.
    e = l = nPolyConn; h = n = -1; b = 1;                                     // kk = nPoly-j.
  }
  else if(vert0 == hexaNodeIDsGrid[ind7] && vert1 == hexaNodeIDsGrid[ind3] && // ii = nPoly-k.
          vert2 == hexaNodeIDsGrid[ind2] && vert3 == hexaNodeIDsGrid[ind6]) { // jj = i.
    a = l = nPolyConn; d = n = -1; f = 1;                                     // kk = nPoly-j.
  }

  else {
    verticesDontMatch = true;
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesQuadrilateralAdjacentHexahedron." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Loop over the DOFs of the original hexahedron to create the connectivity
        of the hexahedron that corresponds to the new numbering. ---*/
  const unsigned short nn2 = (nPolyConn+1)*(nPolyConn+1);
  unsigned short ind = 0;
  for(unsigned short k=0; k<=nPolyConn; ++k) {
    for(unsigned short j=0; j<=nPolyConn; ++j) {
      for(unsigned short i=0; i<=nPolyConn; ++i, ++ind) {

        /*--- Determine the ii, jj and kk indices of the new numbering, convert it to
              a 1D index and shore the modified index in modConnHexa. ---*/
        unsigned short ii   = a + i*b + j*c + k*d;
        unsigned short jj   = e + i*f + j*g + k*h;
        unsigned short kk   = l + i*m + j*n + k*o;
        unsigned short iind = kk*nn2 + jj*(nPolyConn+1) + ii;

        modConnHexa[iind] = connHexa[ind];
      }
    }
  }

  /*--- The quad corresponds to face 0 of the hexahedron. Hence the first
        nn2 entries in modConnHexa are the DOFs of the quad. Copy
        these entries from modConnHexa. ---*/
  for(unsigned short i=0; i<nn2; ++i)
    modConnQuad[i] = modConnHexa[i];
}

void CMeshFEM_DG::CreateConnectivitiesQuadrilateralAdjacentPrism(
                                const unsigned long         *cornerPointsQuad,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &prismNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connPrism,
                                bool                        &swapFaceInElement,
                                unsigned long               *modConnQuad,
                                unsigned long               *modConnPrism) {

  /* Determine the indices of the six corner points of the prism. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+2)/2 -1;
  const unsigned short ind3 = (nPolyGrid+1)*(nPolyGrid+2)*nPolyGrid/2;
  const unsigned short ind4 = ind1 + ind3;
  const unsigned short ind5 = ind2 + ind3;

  /* Easier storage of the four corner points of the quad in the new numbering. */
  const unsigned long vert0 = cornerPointsQuad[0];
  const unsigned long vert1 = cornerPointsQuad[1];
  const unsigned long vert2 = cornerPointsQuad[2];
  const unsigned long vert3 = cornerPointsQuad[3];

  /*--- There exists a linear mapping from the indices of the numbering used in
        prismNodeIDsGrid to the indices of the target numbering. This mapping is
        of the form ii = a + b*i + c*j, jj = d + e*i + f*j and kk = g + h*k,
        where ii,jj,kk are the indices of the new numbering and i,j,k the indices
        of the numbering used in in prismNodeIDsGrid (and connPrism). Note that
        the k-direction can at most be reversed compared to the original definition.
        This is due to the way the basis functions are defined for a prism. As a
        consequence also the variable swapFaceInElement is needed to cover all
        possible situations. The values of the coefficients a,b,c,d,e,f,g,h depend
        on how the corner points of the quad coincide with the corner points of the
        prism. This is determined below. The bool verticesDontMatch is there to
        check if vertices do not match. This should not happen, but it is checked
        for security. ---*/
  signed short a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0;
  bool verticesDontMatch = false;

  if(vert0 == prismNodeIDsGrid[ind0] && vert1 == prismNodeIDsGrid[ind1] &&   // ii = i.
     vert2 == prismNodeIDsGrid[ind4] && vert3 == prismNodeIDsGrid[ind3]) {   // jj = j.
    b = f = h = 1; swapFaceInElement = false;                                // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind0] && vert1 == prismNodeIDsGrid[ind3] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind4] && vert3 == prismNodeIDsGrid[ind1]) {   // jj = j.
    b = f = h = 1; swapFaceInElement = true;                                      // kk = k. Plus swap.
  }
  else if(vert0 == prismNodeIDsGrid[ind0] && vert1 == prismNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind5] && vert3 == prismNodeIDsGrid[ind3]) {   // jj = i.
    c = e = h = 1; swapFaceInElement = false;                                     // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind0] && vert1 == prismNodeIDsGrid[ind3] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind5] && vert3 == prismNodeIDsGrid[ind2]) {   // jj = i.
    c = e = h = 1; swapFaceInElement = true;                                      // kk = k. Plus swap.
  }

  else if(vert0 == prismNodeIDsGrid[ind1] && vert1 == prismNodeIDsGrid[ind0] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind3] && vert3 == prismNodeIDsGrid[ind4]) {   // jj = j.
    a = nPolyConn; b = c = -1; f = h = 1; swapFaceInElement = false;              // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind1] && vert1 == prismNodeIDsGrid[ind4] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind3] && vert3 == prismNodeIDsGrid[ind0]) {   // jj = j.
    a = nPolyConn; b = c = -1; f = h = 1; swapFaceInElement = true;               // kk = k. Plus swap.
  }
  else if(vert0 == prismNodeIDsGrid[ind1] && vert1 == prismNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind5] && vert3 == prismNodeIDsGrid[ind4]) {   // jj = nPoly-i-j.
    d = nPolyConn; e = f = -1; c = h = 1; swapFaceInElement = false;              // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind1] && vert1 == prismNodeIDsGrid[ind4] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind5] && vert3 == prismNodeIDsGrid[ind2]) {   // jj = nPoly-i-j.
    d = nPolyConn; e = f = -1; c = h = 1; swapFaceInElement = true;               // kk = k. Plus swap.
  }

  else if(vert0 == prismNodeIDsGrid[ind2] && vert1 == prismNodeIDsGrid[ind0] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind3] && vert3 == prismNodeIDsGrid[ind5]) {   // jj = i.
    a = nPolyConn; b = c = -1; e = h = 1; swapFaceInElement = false;              // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind2] && vert1 == prismNodeIDsGrid[ind5] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind3] && vert3 == prismNodeIDsGrid[ind0]) {   // jj = i.
    a = nPolyConn; b = c = -1; e = h = 1; swapFaceInElement = true;               // kk = k. Plus swap.
  }
  else if(vert0 == prismNodeIDsGrid[ind2] && vert1 == prismNodeIDsGrid[ind1] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind4] && vert3 == prismNodeIDsGrid[ind5]) {   // jj = nPoly-i-j.
    d = nPolyConn; e = f = -1; b = h = 1; swapFaceInElement = false;              // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind2] && vert1 == prismNodeIDsGrid[ind5] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind4] && vert3 == prismNodeIDsGrid[ind1]) {   // jj = nPoly-i-j.
    d = nPolyConn; e = f = -1; b = h = 1; swapFaceInElement = true;               // kk = k. Plus swap.
  }

  else if(vert0 == prismNodeIDsGrid[ind3] && vert1 == prismNodeIDsGrid[ind4] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind1] && vert3 == prismNodeIDsGrid[ind0]) {   // jj = j.
    g = nPolyConn; b = f = 1; h = -1; swapFaceInElement = false;                  // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind3] && vert1 == prismNodeIDsGrid[ind0] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind1] && vert3 == prismNodeIDsGrid[ind4]) {   // jj = j.
    g = nPolyConn; b = f = 1; h = -1; swapFaceInElement = true;                   // kk = nPoly-k. Plus swap.
  }
  else if(vert0 == prismNodeIDsGrid[ind3] && vert1 == prismNodeIDsGrid[ind5] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind2] && vert3 == prismNodeIDsGrid[ind0]) {   // jj = i.
    g = nPolyConn; c = e = 1; h = -1; swapFaceInElement = false;                  // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind3] && vert1 == prismNodeIDsGrid[ind0] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind2] && vert3 == prismNodeIDsGrid[ind5]) {   // jj = i.
    g = nPolyConn; c = e = 1; h = -1; swapFaceInElement = true;                   // kk = nPoly-k. Plus swap.
  }

  else if(vert0 == prismNodeIDsGrid[ind4] && vert1 == prismNodeIDsGrid[ind3] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind0] && vert3 == prismNodeIDsGrid[ind1]) {   // jj = j.
    a = g = nPolyConn; b = c = h = -1; f = 1; swapFaceInElement = false;          // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind4] && vert1 == prismNodeIDsGrid[ind1] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind0] && vert3 == prismNodeIDsGrid[ind3]) {   // jj = j.
    a = g = nPolyConn; b = c = h = -1; f = 1; swapFaceInElement = true;           // kk = nPoly-k. Plus swap.
  }
  else if(vert0 == prismNodeIDsGrid[ind4] && vert1 == prismNodeIDsGrid[ind5] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind2] && vert3 == prismNodeIDsGrid[ind1]) {   // jj = nPoly-i-j.
    d = g = nPolyConn; e = f = h = -1; c = 1; swapFaceInElement = false;          // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind4] && vert1 == prismNodeIDsGrid[ind1] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind2] && vert3 == prismNodeIDsGrid[ind5]) {   // jj = nPoly-i-j.
    d = g = nPolyConn; e = f = h = -1; c = 1; swapFaceInElement = true;           // kk = nPoly-k. Plus swap.
  }

  else if(vert0 == prismNodeIDsGrid[ind5] && vert1 == prismNodeIDsGrid[ind3] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind0] && vert3 == prismNodeIDsGrid[ind2]) {   // jj = i.
    a = g = nPolyConn; b = c = h = -1; e = 1; swapFaceInElement = false;          // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind5] && vert1 == prismNodeIDsGrid[ind2] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind0] && vert3 == prismNodeIDsGrid[ind3]) {   // jj = i.
    a = g = nPolyConn; b = c = h = -1; e = 1; swapFaceInElement = true;           // kk = nPoly-k. Plus swap.
  }
  else if(vert0 == prismNodeIDsGrid[ind5] && vert1 == prismNodeIDsGrid[ind4] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind1] && vert3 == prismNodeIDsGrid[ind2]) {   // jj = nPoly-i-j.
    d = g = nPolyConn; e = f = h = -1; b = 1; swapFaceInElement = false;          // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind5] && vert1 == prismNodeIDsGrid[ind2] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind1] && vert3 == prismNodeIDsGrid[ind4]) {   // jj = nPoly-i-j.
    d = g = nPolyConn; e = f = h = -1; b = 1; swapFaceInElement = true;           // kk = nPoly-k. Plus swap.
  }

  else {
    verticesDontMatch = true;
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesQuadrilateralAdjacentPrism." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Loop over the DOFs of the original prism to create the connectivity
        of the prism that corresponds to the new numbering. ---*/
  const unsigned short kOff = (nPolyConn+1)*(nPolyConn+2)/2;
  unsigned short ind = 0;
  for(unsigned short k=0; k<=nPolyConn; ++k) {
    for(unsigned short j=0; j<=nPolyConn; ++j) {
      unsigned short uppBoundI = nPolyConn - j;
      for(unsigned short i=0; i<=uppBoundI; ++i, ++ind) {

        /*--- Determine the ii, jj and kk indices of the new numbering, convert it to
              a 1D index and shore the modified index in modConnPrism. ---*/
        unsigned short ii   = a + i*b + j*c;
        unsigned short jj   = d + i*e + j*f;
        unsigned short kk   = g + h*k;
        unsigned short iind = kk*kOff + jj*(nPolyConn+1) + ii - jj*(jj-1)/2;

        modConnPrism[iind] = connPrism[ind];
      }
    }
  }

  /*--- Determine the connectivity of the quadrilateral face. ---*/
  if( swapFaceInElement ) {

    /*--- The parametric coordinates r and s of the quad must be swapped
          w.r.t. to the parametric coordinates of the face of the prism.
          This means that the coordinate r of the quad runs from the base
          triangle to the top triangle of the prism. This corresponds to
          the k-direction of the prism. Hence the s-direction of the quad
          corresponds to the i-direction of the prism. ---*/
    for(unsigned short k=0; k<=nPolyConn; ++k) {
      for(unsigned short i=0; i<=nPolyConn; ++i) {
        const unsigned short iind = i*(nPolyConn+1) + k;
        modConnQuad[iind] = modConnPrism[k*kOff+i];
      }
    }
  }
  else {
    /*--- The parametric coordinates r and s of the quad correspond to the
          parametric coordinates in i- and k-direction of the face of the
          prism. So an easy copy can be made. ---*/
    unsigned short iind = 0;
    for(unsigned short k=0; k<=nPolyConn; ++k) {
      for(unsigned short i=0; i<=nPolyConn; ++i, ++iind) {
        modConnQuad[iind] = modConnPrism[k*kOff+i];
      }
    }
  }
}

void CMeshFEM_DG::CreateConnectivitiesQuadrilateralAdjacentPyramid(
                                const unsigned long         *cornerPointsQuad,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &pyraNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connPyra,
                                unsigned long               *modConnQuad,
                                unsigned long               *modConnPyra) {

  /* Determine the indices of the four corner points of the quadrilateral
     base of the pyramid. Note that the top of the pyramid is not needed
     in the comparison, because only triangular faces are attached to it. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+1) -1;
  const unsigned short ind3 = ind2 - nPolyGrid;

  /* Easier storage of the four corner points of the quad in the new numbering. */
  const unsigned long vert0 = cornerPointsQuad[0];
  const unsigned long vert1 = cornerPointsQuad[1];
  const unsigned long vert2 = cornerPointsQuad[2];
  const unsigned long vert3 = cornerPointsQuad[3];

  /*--- There exists a linear mapping from the indices of the numbering used in
        pyraNodeIDsGrid to the indices of the target numbering. This mapping is of
        the form ii = a + b*i + c*j, jj = d + e*i + f*j and kk = k, where
        ii,jj,kk are the indices of the new numbering and i,j,k the indices of
        the numbering used in in pyraNodeIDsGrid (and connPyra). The values of
        the coefficients a,b,c,d,e,f depend on how the corner points of the quad
        coincide with the corner points of the quadrilateral base of the pyramid.
        This is determined below. The bool verticesDontMatch is there to check
        if vertices do not match. This should not happen, but it is checked for
        security. Note that this mapping is less complicated than for a prism or
        hexahedron, because a pyramid only has one quadrilateral face and hence
        the k-direction has to match. ---*/
  signed short a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
  bool verticesDontMatch = false;

  if(vert0 == pyraNodeIDsGrid[ind0] && vert1 == pyraNodeIDsGrid[ind1] &&   // ii = i.
     vert2 == pyraNodeIDsGrid[ind2] && vert3 == pyraNodeIDsGrid[ind3]) {   // jj = j.
    b = f = 1;
  }
  else if(vert0 == pyraNodeIDsGrid[ind0] && vert1 == pyraNodeIDsGrid[ind3] &&   // ii = j.
          vert2 == pyraNodeIDsGrid[ind2] && vert3 == pyraNodeIDsGrid[ind1]) {   // jj = i.
    c = e = 1;
  }

  else if(vert0 == pyraNodeIDsGrid[ind1] && vert1 == pyraNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == pyraNodeIDsGrid[ind3] && vert3 == pyraNodeIDsGrid[ind0]) {   // jj = nPoly-i.
    d = nPolyConn; c = 1; e = -1;
  }
  else if(vert0 == pyraNodeIDsGrid[ind1] && vert1 == pyraNodeIDsGrid[ind0] &&   // ii = nPoly-i.
          vert2 == pyraNodeIDsGrid[ind3] && vert3 == pyraNodeIDsGrid[ind2]) {   // jj = j.
    a = nPolyConn; b = -1; f = 1;
  }

  else if(vert0 == pyraNodeIDsGrid[ind2] && vert1 == pyraNodeIDsGrid[ind3] &&   // ii = nPoly-i.
          vert2 == pyraNodeIDsGrid[ind0] && vert3 == pyraNodeIDsGrid[ind1]) {   // jj = nPoly-j.
    a = d = nPolyConn; b = f = -1;
  }
  else if(vert0 == pyraNodeIDsGrid[ind2] && vert1 == pyraNodeIDsGrid[ind1] &&   // ii = nPoly-j.
          vert2 == pyraNodeIDsGrid[ind0] && vert3 == pyraNodeIDsGrid[ind3]) {   // jj = nPoly-i.
    a = d = nPolyConn; c = e = -1;
  }

  else if(vert0 == pyraNodeIDsGrid[ind3] && vert1 == pyraNodeIDsGrid[ind0] &&   // ii = nPoly-j.
          vert2 == pyraNodeIDsGrid[ind1] && vert3 == pyraNodeIDsGrid[ind2]) {   // jj = i.
    a = nPolyConn; c = -1; e = 1;
  }
  else if(vert0 == pyraNodeIDsGrid[ind3] && vert1 == pyraNodeIDsGrid[ind2] &&   // ii = i.
          vert2 == pyraNodeIDsGrid[ind1] && vert3 == pyraNodeIDsGrid[ind0]) {   // jj = nPoly-j.
    d = nPolyConn; b = 1; f = -1;
  }

  else {
    verticesDontMatch = true;
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesQuadrilateralAdjacentPyramid." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Loop over the DOFs of the original pyramid to create the connectivity
        of the pyramid that corresponds to the new numbering. Note that the
        outer k-loop is the same for both numberings. ---*/
  unsigned short mPoly    = nPolyConn;
  unsigned short offLevel = 0;

  for(unsigned short k=0; k<=nPolyConn; ++k, --mPoly) {
    unsigned short ind = offLevel;

    /* The variables of a and d in the transformation are actually flexible. */
    /* Create the correct values of a and d for the current k-level.         */
    const signed short aa = a ? mPoly : 0;
    const signed short dd = d ? mPoly : 0;

    /* Loop over the DOFs of the current quadrilateral. */
    for(unsigned short j=0; j<=mPoly; ++j) {
      for(unsigned short i=0; i<=mPoly; ++i, ++ind) {

        /*--- Determine the ii and jj indices of the new numbering, convert it
              to a 1D index and shore the modified index in modConnPyra. ---*/
        unsigned short ii   = aa + i*b + j*c;
        unsigned short jj   = dd + i*e + j*f;
        unsigned short iind = offLevel + jj*(mPoly+1) + ii;

        modConnPyra[iind] = connPyra[ind];
      }
    }

    /* Update offLevel for the next quadrilateral level of the pyramid. */
    offLevel += (mPoly+1)*(mPoly+1);
  }

  /*--- The quad corresponds to face 0 of the pyramid. Hence the first
        nn2 entries in modConnPyra are the DOFs of the quad. Copy
        these entries from modConnPyra. ---*/
  const unsigned short nn2 = (nPolyConn+1)*(nPolyConn+1);
  for(unsigned short i=0; i<nn2; ++i)
    modConnQuad[i] = modConnPyra[i];
}

void CMeshFEM_DG::CreateConnectivitiesTriangleAdjacentPrism(
                                const unsigned long         *cornerPointsTria,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &prismNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connPrism,
                                unsigned long               *modConnTria,
                                unsigned long               *modConnPrism) {

  /* Determine the indices of the six corner points of the prism. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+2)/2 -1;
  const unsigned short ind3 = (nPolyGrid+1)*(nPolyGrid+2)*nPolyGrid/2;
  const unsigned short ind4 = ind1 + ind3;
  const unsigned short ind5 = ind2 + ind3;

  /* Easier storage of the three corner points of the triangle in the new numbering. */
  const unsigned long vert0 = cornerPointsTria[0];
  const unsigned long vert1 = cornerPointsTria[1];
  const unsigned long vert2 = cornerPointsTria[2];

  /*--- There exists a linear mapping from the indices of the numbering used in
        prismNodeIDsGrid to the indices of the target numbering. This mapping is
        of the form ii = a + b*i + c*j, jj = d + e*i + f*j and kk = g + h*k,
        where ii,jj,kk are the indices of the new numbering and i,j,k the indices
        of the numbering used in in prismNodeIDsGrid (and connPrism). Note that
        the k-direction can at most be reversed compared to the original definition.
        This is due to the way the basis functions are defined for a prism.
        The values of the coefficients a,b,c,d,e,f,g,h depend on how the corner
        points of the triangle coincide with the corner points of the prism. This
        is determined below. The bool verticesDontMatch is there to check if vertices
        do not match. This should not happen, but it is checked for security. ---*/
  signed short a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0;
  bool verticesDontMatch = false;

  if(vert0 == prismNodeIDsGrid[ind0] && vert1 == prismNodeIDsGrid[ind1] &&   // ii = i.
     vert2 == prismNodeIDsGrid[ind2]) {                                      // jj = j.
    b = f = h = 1;                                                           // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind0] && vert1 == prismNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind1]) {                                      // jj = i.
    c = e = h = 1;                                                                // kk = k.
  }

  else if(vert0 == prismNodeIDsGrid[ind1] && vert1 == prismNodeIDsGrid[ind0] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind2]) {                                      // jj = j.
    a = nPolyConn; b = c = -1; f = h = 1;                                         // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind1] && vert1 == prismNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind0]) {                                      // jj = nPoly-i-j.
    d = nPolyConn; e = f = -1; c = h = 1;                                         // kk = k.
  }

  else if(vert0 == prismNodeIDsGrid[ind2] && vert1 == prismNodeIDsGrid[ind0] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind1]) {                                      // jj = i.
    a = nPolyConn; b = c = -1; e = h = 1;                                         // kk = k.
  }
  else if(vert0 == prismNodeIDsGrid[ind2] && vert1 == prismNodeIDsGrid[ind1] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind0]) {                                      // jj = nPoly-i-j.
    d = nPolyConn; e = f = -1; b = h = 1;                                         // kk = k.
  }

  else if(vert0 == prismNodeIDsGrid[ind3] && vert1 == prismNodeIDsGrid[ind4] &&   // ii = i.
     vert2 == prismNodeIDsGrid[ind5]) {                                           // jj = j.
    g = nPolyConn; b = f = 1; h = -1;                                             // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind3] && vert1 == prismNodeIDsGrid[ind5] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind4]) {                                      // jj = i.
    g = nPolyConn; c = e = 1; h = -1;                                             // kk = nPoly-k.
  }

  else if(vert0 == prismNodeIDsGrid[ind4] && vert1 == prismNodeIDsGrid[ind3] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind5]) {                                      // jj = j.
    a = g = nPolyConn; b = c = h = -1; f = 1;                                     // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind4] && vert1 == prismNodeIDsGrid[ind5] &&   // ii = j.
          vert2 == prismNodeIDsGrid[ind3]) {                                      // jj = nPoly-i-j.
    d = g = nPolyConn; e = f = h = -1; c = 1;                                     // kk = nPoly-k.
  }

  else if(vert0 == prismNodeIDsGrid[ind5] && vert1 == prismNodeIDsGrid[ind3] &&   // ii = nPoly-i-j.
          vert2 == prismNodeIDsGrid[ind4]) {                                      // jj = i.
    a = g = nPolyConn; b = c = h = -1; e = 1;                                     // kk = nPoly-k.
  }
  else if(vert0 == prismNodeIDsGrid[ind5] && vert1 == prismNodeIDsGrid[ind4] &&   // ii = i.
          vert2 == prismNodeIDsGrid[ind3]) {                                      // jj = nPoly-i-j.
    d = g = nPolyConn; e = f = h = -1; b = 1;                                     // kk = nPoly-k.
  }

  else {
    verticesDontMatch = true;
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesTriangleAdjacentPrism." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Loop over the DOFs of the original prism to create the connectivity
        of the prism that corresponds to the new numbering. ---*/
  const unsigned short kOff = (nPolyConn+1)*(nPolyConn+2)/2;
  unsigned short ind = 0;
  for(unsigned short k=0; k<=nPolyConn; ++k) {
    for(unsigned short j=0; j<=nPolyConn; ++j) {
      unsigned short uppBoundI = nPolyConn - j;
      for(unsigned short i=0; i<=uppBoundI; ++i, ++ind) {

        /*--- Determine the ii, jj and kk indices of the new numbering, convert it to
              a 1D index and shore the modified index in modConnPrism. ---*/
        unsigned short ii   = a + i*b + j*c;
        unsigned short jj   = d + i*e + j*f;
        unsigned short kk   = g + h*k;
        unsigned short iind = kk*kOff + jj*(nPolyConn+1) + ii - jj*(jj-1)/2;

        modConnPrism[iind] = connPrism[ind];
      }
    }
  }

  /*--- The triangle corresponds to face 0 of the prism. Hence the first
        kOff entries in modConnPrism are the DOFs of the triangle. Copy
        these entries from modConnPrism. ---*/
  for(unsigned short i=0; i<kOff; ++i)
    modConnTria[i] = modConnPrism[i];
}

void CMeshFEM_DG::CreateConnectivitiesTriangleAdjacentPyramid(
                                const unsigned long         *cornerPointsTria,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &pyraNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connPyra,
                                bool                        &swapFaceInElement,
                                unsigned long               *modConnTria,
                                unsigned long               *modConnPyra) {

  /* Determine the indices of the five corner points of the pyramid. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+1) -1;
  const unsigned short ind3 = ind2 - nPolyGrid;
  const unsigned short ind4 = (nPolyGrid+1)*(nPolyGrid+2)*(2*nPolyGrid+3)/6 -1;

  /* Check if the top of the pyramid coincides with either corner point 1 or
     corner point 2 of the triangle. Set swapFaceInElement accordingly. */
  if(     cornerPointsTria[1] == pyraNodeIDsGrid[ind4]) swapFaceInElement = true;
  else if(cornerPointsTria[2] == pyraNodeIDsGrid[ind4]) swapFaceInElement = false;
  else {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesTriangleAdjacentPyramid." << endl;
    cout << "Top of the pyramid does not coincide with either corner point 1 or 2." << endl;
    cout << "This should not happen" << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /* Easier storage of the two other corner points of the triangle in the new numbering.
     vert0 always corresponds to cornerPointsTria[0], while vert1 contains the other
     point that is not equal to the top of the pyramid. This depends on swapFaceInElement. */
  const unsigned long vert0 = cornerPointsTria[0];
  const unsigned long vert1 = swapFaceInElement ? cornerPointsTria[2] : cornerPointsTria[1];

  /*--- There exists a linear mapping from the indices of the numbering used in
        pyraNodeIDsGrid to the indices of the target numbering. This mapping is of
        the form ii = a + b*i + c*j, jj = d + e*i + f*j and kk = k, where
        ii,jj,kk are the indices of the new numbering and i,j,k the indices of
        the numbering used in in pyraNodeIDsGrid (and connPyra). The values of
        the coefficients a,b,c,d,e,f depend on how the corner points of the triangle
        coincide with the corner points of the triangular face of the pyramid.
        This is determined below. The bool verticesDontMatch is there to check
        if vertices do not match. This should not happen, but it is checked for
        security. ---*/
  signed short a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
  bool verticesDontMatch = false;

  if(vert0 == pyraNodeIDsGrid[ind0] && vert1 == pyraNodeIDsGrid[ind1]) {   // ii = i.
    b = f = 1;                                                             // jj = j.
  }
  else if(vert0 == pyraNodeIDsGrid[ind0] && vert1 == pyraNodeIDsGrid[ind3]) {   // ii = j.
    c = e = 1;                                                                  // jj = i.
  }

  else if(vert0 == pyraNodeIDsGrid[ind1] && vert1 == pyraNodeIDsGrid[ind2]) {   // ii = j.
    d = nPolyConn; c = 1; e = -1;                                               // jj = nPoly-i.
  }
  else if(vert0 == pyraNodeIDsGrid[ind1] && vert1 == pyraNodeIDsGrid[ind0]) {   // ii = nPoly-i.
    a = nPolyConn; b = -1; f = 1;                                               // jj = j.
  }

  else if(vert0 == pyraNodeIDsGrid[ind2] && vert1 == pyraNodeIDsGrid[ind3]) {   // ii = nPoly-i.
    a = d = nPolyConn; b = f = -1;                                              // jj = nPoly-j.
  }
  else if(vert0 == pyraNodeIDsGrid[ind2] && vert1 == pyraNodeIDsGrid[ind1]) {   // ii = nPoly-j.
    a = d = nPolyConn; c = e = -1;                                              // jj = nPoly-i.
  }

  else if(vert0 == pyraNodeIDsGrid[ind3] && vert1 == pyraNodeIDsGrid[ind0]) {   // ii = nPoly-j.
    a = nPolyConn; c = -1; e = 1;                                               // jj = i.
  }
  else if(vert0 == pyraNodeIDsGrid[ind3] && vert1 == pyraNodeIDsGrid[ind2]) {   // ii = i.
    d = nPolyConn; b = 1; f = -1;                                               // jj = nPoly-j.
  }

  else {
    verticesDontMatch = true;
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    d = nPolyConn; c = 1; e = -1;
    cout << "In function CMeshFEM_DG::CreateConnectivitiesTriangleAdjacentPyramid." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Loop over the DOFs of the original pyramid to create the connectivity
        of the pyramid that corresponds to the new numbering. Note that the
        outer k-loop is the same for both numberings. ---*/
  unsigned short mPoly    = nPolyConn;
  unsigned short offLevel = 0;

  for(unsigned short k=0; k<=nPolyConn; ++k, --mPoly) {
    unsigned short ind = offLevel;

    /* The variables of a and d in the transformation are actually flexible. */
    /* Create the correct values of a and d for the current k-level.         */
    const signed short aa = a ? mPoly : 0;
    const signed short dd = d ? mPoly : 0;

    /* Loop over the DOFs of the current quadrilateral. */
    for(unsigned short j=0; j<=mPoly; ++j) {
      for(unsigned short i=0; i<=mPoly; ++i, ++ind) {

        /*--- Determine the ii and jj indices of the new numbering, convert it
              to a 1D index and shore the modified index in modConnPyra. ---*/
        unsigned short ii   = aa + i*b + j*c;
        unsigned short jj   = dd + i*e + j*f;
        unsigned short iind = offLevel + jj*(mPoly+1) + ii;

        modConnPyra[iind] = connPyra[ind];
      }
    }

    /* Update offLevel for the next quadrilateral level of the pyramid. */
    offLevel += (mPoly+1)*(mPoly+1);
  }

  /*--- Determine the connectivity of the triangular face. ---*/
  if( swapFaceInElement ) {

    /*--- The parametric coordinates r and s of the triangle must be swapped
          w.r.t. to the parametric coordinates of the face of the pyramid.
          This means that the coordinate r of the triangle runs from the base
          to the top of the pyramid. This corresponds to the k-direction of the
          pyramid. Hence the s-direction of the triangle corresponds to the
          i-direction of the pyramid. ---*/
    mPoly    = nPolyConn;
    offLevel = 0;
    for(unsigned short k=0; k<=nPolyConn; ++k, --mPoly) {
      for(unsigned short i=0; i<=mPoly; ++i) {
        unsigned short iind = i*(nPolyConn+1) + k - i*(i-1)/2;
        modConnTria[iind] = modConnPyra[offLevel+i];
      }
      offLevel += (mPoly+1)*(mPoly+1);
    }
  }
  else {
    /*--- The parametric coordinates r and s of the triangle correspond to the
          parametric coordinates in i- and k-direction of the face of the
          pyramid. So an easy copy can be made. ---*/
    mPoly    = nPolyConn;
    offLevel = 0;
    unsigned short iind = 0;
    for(unsigned short k=0; k<=nPolyConn; ++k, --mPoly) {
      for(unsigned short i=0; i<=mPoly; ++i, ++iind) {
        modConnTria[iind] = modConnPyra[offLevel+i];
      }
      offLevel += (mPoly+1)*(mPoly+1);
    }
  }
}

void CMeshFEM_DG::CreateConnectivitiesTriangleAdjacentTetrahedron(
                                const unsigned long         *cornerPointsTria,
                                const unsigned short        nPolyGrid,
                                const vector<unsigned long> &tetNodeIDsGrid,
                                const unsigned short        nPolyConn,
                                const unsigned long         *connTet,
                                unsigned long               *modConnTria,
                                unsigned long               *modConnTet) {

  /* Determine the indices of the four corner points of the tetrahedron. */
  const unsigned short ind0 = 0;
  const unsigned short ind1 = nPolyGrid;
  const unsigned short ind2 = (nPolyGrid+1)*(nPolyGrid+2)/2 -1;
  const unsigned short ind3 = (nPolyGrid+1)*(nPolyGrid+2)*(nPolyGrid+3)/6 -1;

  /* Easier storage of the three corner points of the triangle in the new numbering. */
  const unsigned long vert0 = cornerPointsTria[0];
  const unsigned long vert1 = cornerPointsTria[1];
  const unsigned long vert2 = cornerPointsTria[2];

  /*--- There exists a linear mapping from the indices of the numbering used in
        tetNodeIDsGrid to the indices of the target numbering. This mapping is of
        the form ii = a + b*i + c*j + d*k, jj = e + f*i + g*j + h*k and
        kk = l + m*i + n*j + o*k, where ii,jj,kk are the indices of the new
        numbering and i,j,k the indices of the numbering used in in tetNodeIDsGrid
        (and connTet). The values of the coefficients a,b,c,d,e,f,g,h,l,m,n,o
        depend on how the corner points of the triangle coincide with the corner points
        of the tetrahedron. This is determined below. The bool verticesDontMatch is
        there to check if vertices do not match. This should not happen, but
        it is checked for security. ---*/
  signed short a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0,
               l = 0, m = 0, n = 0, o = 0;
  bool verticesDontMatch = false;

  if(vert0 == tetNodeIDsGrid[ind0] && vert1 == tetNodeIDsGrid[ind1] &&   // ii = i.
     vert2 == tetNodeIDsGrid[ind2]) {                                    // jj = j.
    b = g = o = 1;                                                       // kk = k.
  }
  else if(vert0 == tetNodeIDsGrid[ind0] && vert1 == tetNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == tetNodeIDsGrid[ind1]) {                                    // jj = i.
    c = f = o = 1;                                                            // kk = k.
  }
  else if(vert0 == tetNodeIDsGrid[ind0] && vert1 == tetNodeIDsGrid[ind1] &&   // ii = i.
          vert2 == tetNodeIDsGrid[ind3]) {                                    // jj = k.
    b = h = n = 1;                                                            // kk = j.
  }
  else if(vert0 == tetNodeIDsGrid[ind0] && vert1 == tetNodeIDsGrid[ind3] &&   // ii = k.
          vert2 == tetNodeIDsGrid[ind1]) {                                    // jj = i.
    d = f = n = 1;                                                            // kk = j.
  }
  else if(vert0 == tetNodeIDsGrid[ind0] && vert1 == tetNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == tetNodeIDsGrid[ind3]) {                                    // jj = k.
    c = h = m = 1;                                                            // kk = i.
  }
  else if(vert0 == tetNodeIDsGrid[ind0] && vert1 == tetNodeIDsGrid[ind3] &&   // ii = k.
          vert2 == tetNodeIDsGrid[ind2]) {                                    // jj = j.
    d = g = m = 1;                                                            // kk = i.
  }

  else if(vert0 == tetNodeIDsGrid[ind1] && vert1 == tetNodeIDsGrid[ind0] &&   // ii = nPoly-i-j-k.
          vert2 == tetNodeIDsGrid[ind2]) {                                    // jj = j.
    a = nPolyConn; b = c = d = -1; g = o = 1;                                 // kk = k.
  }
  else if(vert0 == tetNodeIDsGrid[ind1] && vert1 == tetNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == tetNodeIDsGrid[ind0]) {                                    // jj = nPoly-i-j-k.
    e = nPolyConn; f = g = h = -1; c = o = 1;                                 // kk = k.
  }
  else if(vert0 == tetNodeIDsGrid[ind1] && vert1 == tetNodeIDsGrid[ind0] &&   // ii = nPoly-i-j-k.
          vert2 == tetNodeIDsGrid[ind3]) {                                    // jj = k.
    a = nPolyConn; b = c = d = -1; h = n = 1;                                 // kk = j.
  }
  else if(vert0 == tetNodeIDsGrid[ind1] && vert1 == tetNodeIDsGrid[ind3] &&   // ii = k.
          vert2 == tetNodeIDsGrid[ind0]) {                                    // jj = nPoly-i-j-k.
    e = nPolyConn; f = g = h = -1; d = n = 1;                                 // kk = j.
  }
  else if(vert0 == tetNodeIDsGrid[ind1] && vert1 == tetNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == tetNodeIDsGrid[ind3]) {                                    // jj = k.
    l = nPolyConn; m = n = o = -1; c = h = 1;                                 // kk = nPoly-i-j-k.
  }
  else if(vert0 == tetNodeIDsGrid[ind1] && vert1 == tetNodeIDsGrid[ind3] &&   // ii = k.
          vert2 == tetNodeIDsGrid[ind2]) {                                    // jj = j.
    l = nPolyConn; m = n = o = -1; d = g = 1;                                 // kk = nPoly-i-j-k.
  }

  else if(vert0 == tetNodeIDsGrid[ind2] && vert1 == tetNodeIDsGrid[ind0] &&   // ii = nPoly-i-j-k.
          vert2 == tetNodeIDsGrid[ind1]) {                                    // jj = i.
    a = nPolyConn; b = c = d = -1; f = o = 1;                                 // kk = k.
  }
  else if(vert0 == tetNodeIDsGrid[ind2] && vert1 == tetNodeIDsGrid[ind1] &&   // ii = i.
          vert2 == tetNodeIDsGrid[ind0]) {                                    // jj = nPoly-i-j-k.
    e = nPolyConn; f = g = h = -1; b = o = 1;                                 // kk = k.
  }
  else if(vert0 == tetNodeIDsGrid[ind2] && vert1 == tetNodeIDsGrid[ind0] &&   // ii = nPoly-i-j-k.
          vert2 == tetNodeIDsGrid[ind3]) {                                    // jj = k.
    a = nPolyConn; b = c = d = -1; h = m = 1;                                 // kk = i.
  }
  else if(vert0 == tetNodeIDsGrid[ind2] && vert1 == tetNodeIDsGrid[ind3] &&   // ii = k.
          vert2 == tetNodeIDsGrid[ind0]) {                                    // jj = nPoly-i-j-k.
    e = nPolyConn; f = g = h = -1; d = m = 1;                                 // kk = i.
  }
  else if(vert0 == tetNodeIDsGrid[ind2] && vert1 == tetNodeIDsGrid[ind0] &&   // ii = nPoly-i-j-k.
          vert2 == tetNodeIDsGrid[ind3]) {                                    // jj = k.
    a = nPolyConn; b = c = d = -1; h = m = 1;                                 // kk = i.
  }
  else if(vert0 == tetNodeIDsGrid[ind2] && vert1 == tetNodeIDsGrid[ind1] &&   // ii = i.
          vert2 == tetNodeIDsGrid[ind3]) {                                    // jj = k.
    l = nPolyConn; m = n = o = -1; b = h = 1;                                 // kk = nPoly-i-j-k.
  }
  else if(vert0 == tetNodeIDsGrid[ind2] && vert1 == tetNodeIDsGrid[ind3] &&   // ii = k.
          vert2 == tetNodeIDsGrid[ind1]) {                                    // jj = i.
    l = nPolyConn; m = n = o = -1; d = f = 1;                                 // kk = nPoly-i-j-k.
  }

  else if(vert0 == tetNodeIDsGrid[ind3] && vert1 == tetNodeIDsGrid[ind0] &&   // ii = nPoly-i-j-k.
          vert2 == tetNodeIDsGrid[ind1]) {                                    // jj = i.
    a = nPolyConn; b = c = d = -1; f = n = 1;                                 // kk = j.
  }
  else if(vert0 == tetNodeIDsGrid[ind3] && vert1 == tetNodeIDsGrid[ind1] &&   // ii = i.
          vert2 == tetNodeIDsGrid[ind0]) {                                    // jj = nPoly-i-j-k.
    e = nPolyConn; f = g = h = -1; b = n = 1;                                 // kk = j.
  }
  else if(vert0 == tetNodeIDsGrid[ind3] && vert1 == tetNodeIDsGrid[ind0] &&   // ii = nPoly-i-j-k.
          vert2 == tetNodeIDsGrid[ind2]) {                                    // jj = j.
    a = nPolyConn; b = c = d = -1; g = m = 1;                                 // kk = i.
  }
  else if(vert0 == tetNodeIDsGrid[ind3] && vert1 == tetNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == tetNodeIDsGrid[ind0]) {                                    // jj = nPoly-i-j-k.
    e = nPolyConn; f = g = h = -1; c = m = 1;                                 // kk = i.
  }
  else if(vert0 == tetNodeIDsGrid[ind3] && vert1 == tetNodeIDsGrid[ind1] &&   // ii = i.
          vert2 == tetNodeIDsGrid[ind2]) {                                    // jj = j.
    l = nPolyConn; m = n = o = -1; b = g = 1;                                 // kk = nPoly-i-j-k.
  }
  else if(vert0 == tetNodeIDsGrid[ind3] && vert1 == tetNodeIDsGrid[ind2] &&   // ii = j.
          vert2 == tetNodeIDsGrid[ind1]) {                                    // jj = i.
    l = nPolyConn; m = n = o = -1; c = f = 1;                                 // kk = nPoly-i-j-k.
  }

  else {
    verticesDontMatch = true;
  }

  /*--- If non-matching vertices have been found, terminate with an error message. ---*/
  if( verticesDontMatch ) {
    cout << "In function CMeshFEM_DG::CreateConnectivitiesTriangleAdjacentTetrahedron." << endl;
    cout << "Corner vertices do not match. This should not happen." << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }

  /*--- Some constants to convert the (ii,jj,kk) indices to a 1D index. ---*/
  const unsigned short abv1 = (11 + 12*nPolyConn + 3*nPolyConn*nPolyConn);
  const unsigned short abv2 = (2*nPolyConn + 3)*3;
  const unsigned short abv3 = (nPolyConn + 2)*3;

  /*--- Loop over the DOFs of the original tetrahedron to create the connectivity
        of the tetrahedron that corresponds to the new numbering. ---*/
  unsigned short ind = 0;
  for(unsigned short k=0; k<=nPolyConn; ++k) {
    unsigned short uppBoundJ = nPolyConn - k;
    for(unsigned short j=0; j<=uppBoundJ; ++j) {
      unsigned short uppBoundI = nPolyConn - k - j;
      for(unsigned short i=0; i<=uppBoundI; ++i, ++ind) {

        /*--- Determine the ii, jj and kk indices of the new numbering, convert it to
              a 1D index and shore the modified index in modConnTet. ---*/
        unsigned short ii   = a + i*b + j*c + k*d;
        unsigned short jj   = e + i*f + j*g + k*h;
        unsigned short kk   = l + i*m + j*n + k*o;
        unsigned short iind = (abv1*kk + abv2*jj + 6*ii - abv3*kk*kk
                            -  6*kk*jj - 3*jj*jj + kk*kk*kk)/6;

        modConnTet[iind] = connTet[ind];
      }
    }
  }

  /*--- The triangle corresponds to face 0 of the tetrahedron. Hence the first
        nn2 entries in modConnTet are the DOFs of the triangle. Copy these
        entries from modConnTet. ---*/
  const unsigned short nn2 = (nPolyConn+1)*(nPolyConn+2)/2;
  for(unsigned short i=0; i<nn2; ++i)
    modConnTria[i] = modConnTet[i];
}

void CMeshFEM_DG::MetricTermsMatchingFaces(void) {

  /*--------------------------------------------------------------------------*/
  /*--- Step 1: Determine the size of the vector, which stores the metric  ---*/
  /*---         terms of the internal matching face elements. This is a    ---*/
  /*---         large, contiguous vector to increase the performance.      ---*/
  /*---         Each internal face element has pointers, which point to    ---*/
  /*---         particular regions of the large vector.                    ---*/
  /*--------------------------------------------------------------------------*/

  /*--- Loop over the internal faces stored on this rank and determine
        the size of the metric vector for the faces. Note that the normal
        gradient of the basis functions of the adjacent elements must be stored.
        These terms enter the formulation in the Symmetric Interior Penalty
        (SIP) treatment of the viscous terms. ---*/
  unsigned long sizeMetric = 0;
  for(unsigned long i=0; i<matchingFaces.size(); ++i) {

    /* Determine the corresponding standard face element, the number of
       integration points for this face element and the number of DOFS
       of the adjacent volume elements. */
    const unsigned short ind        = matchingFaces[i].indStandardElement;
    const unsigned short nInt       = standardMatchingFacesSol[ind].GetNIntegration();
    const unsigned short nDOFsElem0 = standardMatchingFacesSol[ind].GetNDOFsElemSide0();
    const unsigned short nDOFsElem1 = standardMatchingFacesSol[ind].GetNDOFsElemSide1();

    /* Update the counter sizeMetric by adding the required number for
       this face element. For each integration point the following data
       is stored: - Unit normals + area (nDim+1).
                  - drdx, dsdx, etc. for both sides (2*nDim*nDim)
                  - Normal derivatives of the element basis functions
                    for both sides (nDOFsElem0 + nDOFsElem1. )*/
    sizeMetric += nInt*(nDim + 1 + 2*nDim*nDim + nDOFsElem0 + nDOFsElem1);
  }

  /* Allocate the memory for the vector to store the metric terms. */
  VecMetricTermsInternalMatchingFaces.resize(sizeMetric);

  /*--------------------------------------------------------------------------*/
  /*--- Step 2: Set the pointers for storing the metric terms to the       ---*/
  /*---         correct locations in VecMetricTermsInternalMatchingFaces.  ---*/
  /*--------------------------------------------------------------------------*/

  /* Loop over the internal matching faces. */
  sizeMetric = 0;
  for(unsigned long i=0; i<matchingFaces.size(); ++i) {

    /* Determine the corresponding standard face element and get the
       relevant information from it. */
    const unsigned short ind  = matchingFaces[i].indStandardElement;
    const unsigned short nInt = standardMatchingFacesSol[ind].GetNIntegration();

    const unsigned short nDOFsElemSol0 = standardMatchingFacesSol[ind].GetNDOFsElemSide0();
    const unsigned short nDOFsElemSol1 = standardMatchingFacesSol[ind].GetNDOFsElemSide1();

    /* Set the pointers to the locations in VecMetricTermsInternalMatchingFaces
       where the actual metric data for this matching face is stored. */
    matchingFaces[i].metricNormalsFace = VecMetricTermsInternalMatchingFaces.data()
                                       + sizeMetric;
    sizeMetric += nInt*(nDim+1);

    matchingFaces[i].metricCoorDerivFace0 = VecMetricTermsInternalMatchingFaces.data()
                                          + sizeMetric;
    sizeMetric += nInt*nDim*nDim;

    matchingFaces[i].metricCoorDerivFace1 = VecMetricTermsInternalMatchingFaces.data()
                                          + sizeMetric;
    sizeMetric += nInt*nDim*nDim;

    matchingFaces[i].metricElemSide0 = VecMetricTermsInternalMatchingFaces.data()
                                     + sizeMetric;
    sizeMetric += nInt*nDOFsElemSol0;

    matchingFaces[i].metricElemSide1 = VecMetricTermsInternalMatchingFaces.data()
                                     + sizeMetric;
    sizeMetric += nInt*nDOFsElemSol1;
  }

  /*--------------------------------------------------------------------------*/
  /*--- Step 3: Determine the actual metric data in the integration points ---*/
  /*---         of the faces.                                              ---*/
  /*--------------------------------------------------------------------------*/

  /* Loop over the internal matching faces. */
  for(unsigned long i=0; i<matchingFaces.size(); ++i) {

    /* Determine the corresponding standard face and its number of integration
       points. Note that the standard element of the grid must be used here. */
    const unsigned short ind  = matchingFaces[i].indStandardElement;
    const unsigned short nInt = standardMatchingFacesGrid[ind].GetNIntegration();

    /* Call the function ComputeNormalsFace to compute the unit normals and
       its corresponding area in the integration points. The data from side 0
       is used, but the data from side 1 should give the same result. */
    unsigned short nDOFs = standardMatchingFacesGrid[ind].GetNDOFsFaceSide0();
    const su2double *dr = standardMatchingFacesGrid[ind].GetDrBasisFaceIntegrationSide0();
    const su2double *ds = standardMatchingFacesGrid[ind].GetDsBasisFaceIntegrationSide0();

    ComputeNormalsFace(nInt, nDOFs, dr, ds, matchingFaces[i].DOFsGridFaceSide0,
                       matchingFaces[i].metricNormalsFace);

    /* Compute the derivatives of the parametric coordinates w.r.t. the
       Cartesian coordinates, i.e. drdx, drdy, etc. in the integration points
       on side 0 of the face. */
    nDOFs = standardMatchingFacesGrid[ind].GetNDOFsElemSide0();
    dr = standardMatchingFacesGrid[ind].GetMatDerBasisElemIntegrationSide0();

    ComputeGradientsCoordinatesFace(nInt, nDOFs, dr,
                                    matchingFaces[i].DOFsGridElementSide0,
                                    matchingFaces[i].metricCoorDerivFace0);

    /* Compute the metric terms on side 0 needed for the SIP treatment
       of the viscous terms. Note that now the standard element of the
       solution must be taken. */
    const su2double *dt;
    nDOFs = standardMatchingFacesSol[ind].GetNDOFsElemSide0();
    dr = standardMatchingFacesSol[ind].GetDrBasisElemIntegrationSide0();
    ds = standardMatchingFacesSol[ind].GetDsBasisElemIntegrationSide0();
    dt = standardMatchingFacesSol[ind].GetDtBasisElemIntegrationSide0();

    ComputeMetricTermsSIP(nInt, nDOFs, dr, ds, dt,
                          matchingFaces[i].metricNormalsFace,
                          matchingFaces[i].metricCoorDerivFace0,
                          matchingFaces[i].metricElemSide0);

    /* Compute the derivatives of the parametric coordinates w.r.t. the
       Cartesian coordinates, i.e. drdx, drdy, etc. in the integration points
       on side 1 of the face. */
    nDOFs = standardMatchingFacesGrid[ind].GetNDOFsElemSide1();
    dr = standardMatchingFacesGrid[ind].GetMatDerBasisElemIntegrationSide1();

    ComputeGradientsCoordinatesFace(nInt, nDOFs, dr,
                                    matchingFaces[i].DOFsGridElementSide1,
                                    matchingFaces[i].metricCoorDerivFace1);

    /* Compute the metric terms on side 1 needed for the SIP treatment
       of the viscous terms. Note that now the standard element of the
       solution must be taken. */
    nDOFs = standardMatchingFacesSol[ind].GetNDOFsElemSide1();
    dr = standardMatchingFacesSol[ind].GetDrBasisElemIntegrationSide1();
    ds = standardMatchingFacesSol[ind].GetDsBasisElemIntegrationSide1();
    dt = standardMatchingFacesSol[ind].GetDtBasisElemIntegrationSide1();

    ComputeMetricTermsSIP(nInt, nDOFs, dr, ds, dt,
                          matchingFaces[i].metricNormalsFace,
                          matchingFaces[i].metricCoorDerivFace1,
                          matchingFaces[i].metricElemSide1);
  }
}

void CMeshFEM_DG::MetricTermsSurfaceElements(void) {

  /*--- Compute the metric terms of the internal matching faces. ---*/
  MetricTermsMatchingFaces();

  /*--- Loop over the physical boundaries and compute the metric
        terms of the boundary. ---*/
  for(unsigned short i=0; i<boundaries.size(); ++i) {
    if( !boundaries[i].periodicBoundary )
      MetricTermsBoundaryFaces(&boundaries[i]);
  }
}

void CMeshFEM_DG::MetricTermsVolumeElements(CConfig *config) {

  /*--------------------------------------------------------------------------*/
  /*--- Step 1: Determine the sizes of the vector, which store the metric  ---*/
  /*---         terms and mass matrices of the volume elements. These are  ---*/
  /*---         large, contiguous vectors to increase the performance.     ---*/
  /*---         Each volume element has pointers, which point to a         ---*/
  /*---         particular region of the large vectors.                    ---*/
  /*--------------------------------------------------------------------------*/

  /* Find out whether or not the full mass matrix is needed. This is only the
     case for time accurate simulations. For steady simulations only a lumped
     version is needed in order to be dimensionally consistent. Moreover, for
     implicit time integration schemes the mass matrix itself is needed, while
     for explicit time integration schemes the inverse of the mass matrix is
     much more convenient. Note that for the DG_FEM the mass matrix is local
     to the elements. */
  bool FullMassMatrix, FullInverseMassMatrix, LumpedMassMatrix;
  if(config->GetUnsteady_Simulation() == STEADY ||
     config->GetUnsteady_Simulation() == ROTATIONAL_FRAME) {
    FullMassMatrix   = FullInverseMassMatrix = false;
    LumpedMassMatrix = true;
  }
  else if(config->GetUnsteady_Simulation() == DT_STEPPING_1ST ||
          config->GetUnsteady_Simulation() == DT_STEPPING_2ND ||
          config->GetUnsteady_Simulation() == TIME_SPECTRAL) {
    FullMassMatrix        = LumpedMassMatrix = true;
    FullInverseMassMatrix = false;
  }
  else {
    FullMassMatrix        = LumpedMassMatrix = false;
    FullInverseMassMatrix = true;
  }

  /* Determine the number of metric terms per integration point.
     This depends on the number of spatial dimensions of the problem. */
  const unsigned short nMetricPerPoint = nDim == 3 ? 10 : 5;

  /*--- Loop over the owned volume elements to determine the size of
        the metric vector and the size of the mass matrix vector. */
  unsigned long sizeMetric = 0, sizeMassMatrix = 0;
  for(unsigned long i=0; i<nVolElemOwned; ++i) {

    /* Determine the number of metric coefficients needed for this element
       and update the counter sizeMetric accordingly. */
    const unsigned short ind = volElem[i].indStandardElement;
    sizeMetric += nMetricPerPoint*standardElementsSol[ind].GetNIntegration();

    /* Update the counter sizeMassMatrix, depending on which types of the
       mass matrix is needed. Note that it is not possible to store both
       the mass matrix and the inverse of the mass matrix. */
    if(FullMassMatrix || FullInverseMassMatrix)
      sizeMassMatrix += volElem[i].nDOFsSol * volElem[i].nDOFsSol;

    if( LumpedMassMatrix ) sizeMassMatrix += volElem[i].nDOFsSol;
  }

  /* Allocate the memory for the vectors to store the metric terms and
     the mass matrices of the volume elements. */
  VecMetricTermsElements.resize(sizeMetric);
  VecMassMatricesElements.resize(sizeMassMatrix);

  /*--------------------------------------------------------------------------*/
  /*--- Step 2: Determine the metric terms, drdx, drdy, drdz, dsdx, etc.   ---*/
  /*---         and the Jacobian in the integration points of the owned    ---*/
  /*---         volume elements.                                           ---*/
  /*--------------------------------------------------------------------------*/

#if defined (HAVE_CBLAS) || defined(HAVE_MKL)

  /* In case the Lapack/Blas routines must be used, some help arrays must
     be allocated. Determine the sizes of these help arrays by looping
     over the standard volume elements. */
  unsigned long sizeResult = 0, sizeRHS = 0;

  for(unsigned long i=0; i<standardElementsGrid.size(); ++i) {
    unsigned long thisSize = standardElementsGrid[i].GetNIntegration();
    sizeResult = max(sizeResult, thisSize);

    thisSize = standardElementsGrid[i].GetNDOFs();
    sizeRHS  = max(sizeRHS, thisSize);
  }

  sizeResult *= nDim*nDim;
  sizeRHS    *= nDim;

  /* Allocate the memory. In case the MKL is used, a specialized allocation
     is used to optimize performance. */
  su2double *vecResult, *vecRHS;

#ifdef HAVE_MKL
  vecResult = (su2double *) mkl_malloc(sizeResult*sizeof(su2double), 64);
  vecRHS    = (su2double *) mkl_malloc(sizeRHS   *sizeof(su2double), 64);
#else
  vector<su2double> helpVecResult(sizeResult), helpVecRHS(sizeRHS);
  vecResult = helpVecResult.data();
  vecRHS    = helpVecRHS.data();
#endif

#endif

  /* Loop over the owned volume elements. */
  sizeMetric = 0;
  for(unsigned long i=0; i<nVolElemOwned; ++i) {

    /* Easier storage of the index of the corresponding standard element
       and determine the number of integration points as well as the number
       of DOFs for the grid. */
    const unsigned short ind   = volElem[i].indStandardElement;
    const unsigned short nInt  = standardElementsGrid[ind].GetNIntegration();
    const unsigned short nDOFs = volElem[i].nDOFsGrid;

    /* Store the pointer for the metric terms for this element and update
       sizeMetric for the next element. */
    volElem[i].metricTerms = VecMetricTermsElements.data() + sizeMetric;
    sizeMetric += nMetricPerPoint*nInt;

    /* Get the pointer to the matrix storage of the basis functions
       and its derivatives. The first nDOFs*nInt entries of this matrix
       correspond to the interpolation data to the integration points
       and are not needed. Hence this part is skipped. */
    const su2double *matBasisInt    = standardElementsGrid[ind].GetMatBasisFunctionsIntegration();
    const su2double *matDerBasisInt = &matBasisInt[nDOFs*nInt];

    /* Allocate the memory for the result vector. In case the MKL is used
       a specialized allocation is used to optimize performance. */
    su2double *vecResult;

#ifdef HAVE_MKL
    vecResult = (su2double *) mkl_malloc(nInt*nDim*nDim*sizeof(su2double), 64);
#else
    vector<su2double> helpVecResult(nInt*nDim*nDim);
    vecResult = helpVecResult.data();
#endif

    /* Compute the gradient of the coordinates w.r.t. the parametric
       coordinates for this element. */
    ComputeGradientsCoorWRTParam(nInt, nDOFs, matDerBasisInt,
                                 volElem[i].nodeIDsGrid.data(),
                                 vecResult);

    /*--- Convert the dxdr, dydr, etc. to the required metric terms.
          Make a distinction between 2D and 3D. ---*/
    unsigned short ii = 0;
    switch( nDim ) {
      case 2: {

        /* 2D computation. Store the offset between the r and s derivatives. */
        const unsigned short off = 2*nInt;

        /* Loop over the integration points and store the metric terms. */
        for(unsigned short j=0; j<nInt; ++j) {
          const unsigned short jx = 2*j; const unsigned short jy = jx+1;
          const su2double dxdr = vecResult[jx],     dydr = vecResult[jy];
          const su2double dxds = vecResult[jx+off], dyds = vecResult[jy+off];

          volElem[i].metricTerms[ii++] =  dxdr*dyds - dxds*dydr; // J
          volElem[i].metricTerms[ii++] =  dyds;   // J drdx
          volElem[i].metricTerms[ii++] = -dxds;   // J drdy
          volElem[i].metricTerms[ii++] = -dydr;   // J dsdx
          volElem[i].metricTerms[ii++] =  dxdr;   // J dsdy
        }

        break;
      }

      case 3: {
        /* 3D computation. Store the offset between the r and s and r and t derivatives. */
        const unsigned short offS = 3*nInt, offT = 6*nInt;

        /* Loop over the integration points and store the metric terms. */
        for(unsigned short j=0; j<nInt; ++j) {
          const unsigned short jx = 3*j; const unsigned short jy = jx+1, jz = jx+2;
          const su2double dxdr = vecResult[jx],      dydr = vecResult[jy],      dzdr = vecResult[jz];
          const su2double dxds = vecResult[jx+offS], dyds = vecResult[jy+offS], dzds = vecResult[jz+offS];
          const su2double dxdt = vecResult[jx+offT], dydt = vecResult[jy+offT], dzdt = vecResult[jz+offT];

          volElem[i].metricTerms[ii++] = dxdr*(dyds*dzdt - dzds*dydt)
                                       - dxds*(dydr*dzdt - dzdr*dydt)
                                       + dxdt*(dydr*dzds - dzdr*dyds); // J

          volElem[i].metricTerms[ii++] = dyds*dzdt - dzds*dydt;  // J drdx
          volElem[i].metricTerms[ii++] = dzds*dxdt - dxds*dzdt;  // J drdy
          volElem[i].metricTerms[ii++] = dxds*dydt - dyds*dxdt;  // J drdz

          volElem[i].metricTerms[ii++] = dzdr*dydt - dydr*dzdt;  // J dsdx
          volElem[i].metricTerms[ii++] = dxdr*dzdt - dzdr*dxdt;  // J dsdy
          volElem[i].metricTerms[ii++] = dydr*dxdt - dxdr*dydt;  // J dsdz

          volElem[i].metricTerms[ii++] = dydr*dzds - dzdr*dyds;  // J dtdx
          volElem[i].metricTerms[ii++] = dzdr*dxds - dxdr*dzds;  // J dtdy
          volElem[i].metricTerms[ii++] = dxdr*dyds - dydr*dxds;  // J dtdz
        }

        break;
      }
    }

    /* If the MKL is used, the help array must be deallocated. */
#ifdef HAVE_MKL
    mkl_free(vecResult);
#endif

    /* Check for negative Jacobians in the integrations points. */
    for(unsigned short j=0; j<nInt; ++j) {
      if(volElem[i].metricTerms[nMetricPerPoint*j] <= 0.0) {
        cout << "Negative Jacobian found" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }
    }

  }

  /*--------------------------------------------------------------------------*/
  /*--- Step 3: Determine the mass matrix (or its inverse) and/or the      ---*/
  /*---         lumped mass matrix.                                        ---*/
  /*--------------------------------------------------------------------------*/

  /* Loop over the owned volume elements. */
  sizeMassMatrix = 0;
  for(unsigned long i=0; i<nVolElemOwned; ++i) {

    /* Easier storage of the index of the corresponding standard element and
       determine the number of integration points, the number of DOFs for the
       solution, the Lagrangian interpolation functions in the integration points
       and the weights in the integration points. */
    const unsigned short ind   = volElem[i].indStandardElement;
    const unsigned short nInt  = standardElementsSol[ind].GetNIntegration();
    const unsigned short nDOFs = volElem[i].nDOFsSol;
    const su2double      *lag  = standardElementsSol[ind].GetBasisFunctionsIntegration();
    const su2double      *w    = standardElementsSol[ind].GetWeightsIntegration();

    /*--- Check if the mass matrix or its inverse must be computed. ---*/
    if(FullMassMatrix || FullInverseMassMatrix) {

      /* Store the pointer for the mass matrix for this element and update
         the counter sizeMassMatrix. */
      volElem[i].massMatrix = VecMassMatricesElements.data() + sizeMassMatrix;
      sizeMassMatrix       += nDOFs*nDOFs;

      /*--- Double loop over the DOFs to create the local mass matrix. ---*/
      unsigned short ll = 0;
      for(unsigned short k=0; k<nDOFs; ++k) {
        for(unsigned short j=0; j<nDOFs; ++j, ++ll) {
          volElem[i].massMatrix[ll] = 0.0;

          /* Loop over the integration point to create the actual value of entry
             (k,j) of the local mass matrix. */
          for(unsigned short l=0; l<nInt; ++l)
            volElem[i].massMatrix[ll] += volElem[i].metricTerms[l*nMetricPerPoint]
                                       * w[l]*lag[l*nDOFs+k]*lag[l*nDOFs+j];
        }
      }

      /*--- Check if the inverse of mass matrix is needed. ---*/
      if( FullInverseMassMatrix ) {

        /*--- Check if the LAPACK/MKL can be used to compute the inverse. ---*/

#if defined (HAVE_LAPACK) || defined(HAVE_MKL)

        /* The inverse can be computed using the Lapack routines LAPACKE_dpotrf
           and LAPACKE_dpotri. As the mass matrix is positive definite, a
           Cholesky decomposition is used, which is much more efficient than
           a standard inverse. */
        lapack_int errorCode;
        errorCode = LAPACKE_dpotrf(LAPACK_ROW_MAJOR, 'U', nDOFs,
                                    volElem[i].massMatrix, nDOFs);
        if(errorCode != 0) {
          cout << endl;
          cout << "In function CMeshFEM_DG::MetricTermsVolumeElements." << endl;
          if(errorCode < 0)  {
            cout << "Something wrong when calling LAPACKE_dpotrf. Error code: "
                 << errorCode << endl;
          }
          else {
            cout << "Mass matrix not positive definite. " << endl;
            cout << "This is most likely caused by a too low accuracy of the quadrature rule," << endl;
            cout << "possibly combined with a low quality element." << endl;
            cout << "Increase the accuracy of the quadrature rule." << endl;
          }
          cout << endl;
#ifndef HAVE_MPI
          exit(EXIT_FAILURE);
#else
          MPI_Abort(MPI_COMM_WORLD,1);
          MPI_Finalize();
#endif
        }

        errorCode = LAPACKE_dpotri(LAPACK_ROW_MAJOR, 'U', nDOFs,
                                   volElem[i].massMatrix, nDOFs);
        if(errorCode != 0) {
          cout << endl;
          cout << "In function CMeshFEM_DG::MetricTermsVolumeElements." << endl;
          if(errorCode < 0) {
            cout << "Something wrong when calling LAPACKE_dpotri. Error code: "
                 << errorCode << endl;
          }
          else {
            cout << "Mass matrix is singular. " << endl;
            cout << "The is most likely caused by a too low accuracy of the quadrature rule, " << endl;
            cout << "possibly combined with a low quality element." << endl;
            cout << "Increase the accuracy of the quadrature rule." << endl;
          }
          cout << endl;
#ifndef HAVE_MPI
          exit(EXIT_FAILURE);
#else
          MPI_Abort(MPI_COMM_WORLD,1);
          MPI_Finalize();
#endif
        }

        /* The Lapack routines for a Cholesky decomposition only store the upper
           part of the matrix. Copy the data to the lower part. */
        for(unsigned short k=0; k<nDOFs; ++k) {
          for(unsigned short j=(k+1); j<nDOFs; ++j) {
            volElem[i].massMatrix[j*nDOFs+k] = volElem[i].massMatrix[k*nDOFs+j];
          }
        }
#else
        /* No support for Lapack. Hence an internal routine is used.
           This does not all the checking the Lapack routine does. */
        vector<su2double> matA(nDOFs*nDOFs);
        memcpy(matA.data(), volElem[i].massMatrix, nDOFs*nDOFs*sizeof(su2double));

        FEMStandardElementBaseClass::InverseMatrix(nDOFs, matA);
        memcpy(volElem[i].massMatrix, matA.data(), nDOFs*nDOFs*sizeof(su2double));
#endif
      }
    }

    /*--- Check if the lumped mass matrix is needed. ---*/
    if( LumpedMassMatrix ) {

      /* Store the pointer for the lumped mass matrix for this element and update
         the counter sizeMassMatrix. */
      volElem[i].lumpedMassMatrix = VecMassMatricesElements.data() + sizeMassMatrix;
      sizeMassMatrix             += nDOFs;

      /*--- Loop over the DOFs to compute the diagonal elements of the local mass
            matrix. Determine the trace as well. ---*/
      su2double traceMass = 0.0;
      for(unsigned short j=0; j<nDOFs; ++j) {
        volElem[i].lumpedMassMatrix[j] = 0.0;

        /* Loop over the integration point to create the actual value. */
        for(unsigned short l=0; l<nInt; ++l)
          volElem[i].lumpedMassMatrix[j] += volElem[i].metricTerms[l*nMetricPerPoint]
                                          * w[l]*lag[l*nDOFs+j]*lag[l*nDOFs+j];
        traceMass += volElem[i].lumpedMassMatrix[j];
      }

      /* Compute the volume of the element and divide it by the trace of the
         mass matrix. This is the scaling factor for currently stored diagonal
         entries of the mass matrix to obtain the lumped version. */
      su2double volume = 0.0;
      for(unsigned short l=0; l<nInt; ++l)
        volume += w[l]*volElem[i].metricTerms[l*nMetricPerPoint];
      volume /= traceMass;

      /* Compute the values of the lumped mass matrix for the DOFs. */
      for(unsigned short j=0; j<nDOFs; ++j)
        volElem[i].lumpedMassMatrix[j] *= volume;
    }
  }
}