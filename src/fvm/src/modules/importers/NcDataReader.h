//
// C++ Interface: NcDataREADER
//
// Description: 
//
//
// Author: yildirim,,, <yildirim@prism>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef NCDATAREADER_H
#define NCDATAREADER_H

#include <string>
#include <vector>

#include "Mesh.h"


using namespace std;

/**
	@author yildirim,,, <yildirim@prism>
*/

class NcFile;
class NcDim;
class NcVar;


typedef   shared_ptr< StorageSite >  StorageSitePtr; 
typedef   shared_ptr< CRConnectivity > CRConnectivityPtr;


class NcDataReader {

public :

    NcDataReader( const string& fname );
    
    void    read();

    ~NcDataReader();

private :

    void  init();
    void  setNcFile();
    void  getDims();
    void  getVars();

    void  get_var_values();
    void  allocate_vars();
    void  get_bndry_vals();
    void  get_interface_vals();
    void  get_coord_vals();
    void  get_connectivity_vals();


    void  meshList();
    void  storage_sites( int id );
    void  boundary_faces( int id );
    void  interfaces( int id );
    void  coords( int id );
    void  face_cells( int id );
    void  face_nodes( int id );
//    void  mappers( int id );


    string _fname;


    //netcdf variables    
    NcFile   *_ncFile;
    int  _nmesh;
    int  _nBoun;
    int  _charSize;
    int  _nInterface;
    int  _nnodes;

    int   _nfaceRow;
    int   _nfaceCellsCol;
    int   _nfaceNodesCol;



    NcVar *_dimension;
    NcVar *_meshID;
    NcVar *_facesCount;
    NcVar *_cellsCount;
    NcVar *_ghostCellsCount;
    NcVar *_nodesCount;
    NcVar *_mapCount;
    NcVar *_interiorFacesGroup;

    NcVar*  _boundaryGroup;
    NcVar*  _boundarySize;
    NcVar*  _boundaryOffset;
    NcVar*  _boundaryID;
    NcVar*  _boundaryType;


    NcVar*  _interfaceGroup;
    NcVar*  _interfaceSize;
    NcVar*  _interfaceOffset;
    NcVar*  _interfaceID;
  
    NcVar*  _x;
    NcVar*  _y;
    NcVar*  _z;

    NcVar* _faceCellsRow;
    NcVar* _faceCellsCol;
    NcVar* _faceNodesRow;
    NcVar* _faceNodesCol;


    int*  _dimensionVals;
    int*  _meshIDVals;

    int *_facesCountVals;
    int *_cellsCountVals;
    int *_ghostCellsCountVals;
    int *_nodesCountVals;
    int *_mapCountVals;
    int *_interiorFacesGroupVals;


    int *_boundaryGroupVals;
    int *_boundarySizeVals;
    int *_boundaryOffsetVals;
    int * _boundaryIDVals;
    vector<char*>  _boundaryTypeVals;

    int  *_interfaceGroupVals;
    int  *_interfaceSizeVals;
    int  *_interfaceOffsetVals;
    int  *_interfaceIDVals;

    double  *_xVals;
    double  *_yVals;
    double  *_zVals;
    
     int  *_faceCellsRowVals;
     int  *_faceCellsColVals;
     int  *_faceNodesRowVals;
     int  *_faceNodesColVals;
    


     MeshList   _meshList;
};

#endif
