//
// C++ Implementation: NcDataReader
//
// Description: 
//
//
// Author: yildirim,,, <yildirim@prism>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include <iostream>
#include <cassert>
#include <algorithm>
#include <numeric>

#include "NcDataReader.h"
#include "netcdfcpp.h"
#include "mpi.h"



NcDataReader::NcDataReader( const string& fname )
:_fname( fname )
{
    init();
}


NcDataReader::~NcDataReader()
{

    if ( _dimensionVals ) delete [] _dimensionVals;
    if ( _meshIDVals    ) delete [] _meshIDVals;

    if ( _facesCountVals ) delete []  _facesCountVals;
    if ( _cellsCountVals ) delete [] _cellsCountVals;
    if ( _ghostCellsCountVals ) delete [] _ghostCellsCountVals;
    if (_nodesCountVals ) delete [] _nodesCountVals;
    if ( _mapCountVals  ) delete [] _mapCountVals;
    if ( _interiorFacesGroupVals ) delete [] _interiorFacesGroupVals;

    if ( _boundaryGroupVals )  delete [] _boundaryGroupVals;
    if ( _boundarySizeVals )   delete [] _boundarySizeVals;
    if ( _boundaryOffsetVals ) delete [] _boundaryOffsetVals;
    if ( _boundaryIDVals )     delete [] _boundaryIDVals;

    vector< char* >::iterator it_char;
    for ( it_char = _boundaryTypeVals.begin(); it_char != _boundaryTypeVals.end(); it_char++ )
         delete [] *it_char;


    if ( _interfaceGroupVals )  delete [] _interfaceGroupVals;
    if ( _interfaceSizeVals )   delete [] _interfaceSizeVals;
    if ( _interfaceOffsetVals ) delete [] _interfaceOffsetVals;
    if ( _interfaceIDVals )     delete [] _interfaceIDVals;
  
    if ( _xVals )   delete [] _xVals;
    if ( _yVals )   delete [] _yVals;
    if ( _zVals )   delete [] _zVals;

    if ( _faceCellsRowVals ) delete [] _faceCellsRowVals;
    if ( _faceNodesRowVals ) delete [] _faceNodesRowVals;
    if ( _faceCellsColVals ) delete [] _faceCellsColVals;
    if ( _faceNodesColVals ) delete [] _faceNodesColVals;
   

    if ( _ncFile        ) delete _ncFile;

}



void
NcDataReader::read()
{
   setNcFile();
   getDims();
   getVars();
   get_var_values();

   meshList();

}


                     //PRIVATE FUNCTIONS
void
NcDataReader::init()
{
    _ncFile = NULL;
    _dimensionVals = NULL;
    _meshIDVals  = NULL;
    _facesCountVals = NULL;
    _cellsCountVals = NULL;
    _ghostCellsCountVals = NULL;
    _nodesCountVals = NULL;
    _mapCountVals = NULL;
    _interiorFacesGroupVals = NULL;

    _boundaryGroupVals = NULL;
    _boundarySizeVals  = NULL;
    _boundaryOffsetVals= NULL;
    _boundaryIDVals    = NULL;

    _interfaceGroupVals  = NULL;
    _interfaceSizeVals   = NULL;
    _interfaceOffsetVals = NULL;
    _interfaceIDVals     = NULL;

    _xVals = NULL;
    _yVals = NULL;
    _zVals = NULL;

    _faceCellsRowVals = NULL;
    _faceNodesRowVals = NULL;
    _faceCellsColVals = NULL;
    _faceCellsColVals = NULL;
}		   

//Setting NcFile
void
NcDataReader::setNcFile()
{
   assert( !_ncFile );
   _ncFile = new NcFile( _fname.c_str(), NcFile::ReadOnly );
   assert ( _ncFile->is_valid() );

}  


//Getting dimension from NcFile
void
NcDataReader::getDims()
{
   _nmesh    = _ncFile->get_dim("nmesh")->size();
   _nBoun    = _ncFile->get_dim("boun_type_dim")->size();
   _charSize = _ncFile->get_dim("char_dim")->size();
   _nInterface = _ncFile->get_dim("nInterface")->size();
   _nnodes     = _ncFile->get_dim("nnodes")->size();
   _nfaceRow   = _ncFile->get_dim("nface_row")->size();
   _nfaceCellsCol = _ncFile->get_dim("nfaceCells_col")->size();
   _nfaceNodesCol = _ncFile->get_dim("nfaceNodes_col")->size();


}

//getting NCVar-s
void 
NcDataReader::getVars()
{
    _dimension =  _ncFile->get_var("dimension");
    _meshID    =  _ncFile->get_var("mesh_id"  ); 
    _facesCount = _ncFile->get_var("faces_count");
    _cellsCount = _ncFile->get_var("cells_count");
    _ghostCellsCount = _ncFile->get_var("ghost_cells_count");
    _nodesCount = _ncFile->get_var("nodes_count");
    _mapCount   = _ncFile->get_var("map_count");
    _interiorFacesGroup = _ncFile->get_var("interior_faces_group");

    _boundaryGroup   = _ncFile->get_var("boundary_group");
    _boundarySize    = _ncFile->get_var("boundary_size");
    _boundaryOffset  = _ncFile->get_var("boundary_offset");
    _boundaryID      = _ncFile->get_var("boundary_id"); 
    _boundaryType    = _ncFile->get_var("boundary_type"); 


    _interfaceGroup   = _ncFile->get_var("interface_group");
    _interfaceSize    = _ncFile->get_var("interface_size");
    _interfaceOffset  = _ncFile->get_var("interface_offset");
    _interfaceID      = _ncFile->get_var("interface_id"); 

    _x = _ncFile->get_var("x");
    _y = _ncFile->get_var("y");
    _z = _ncFile->get_var("z");

    _faceCellsRow = _ncFile->get_var("face_cells_row");
    _faceCellsCol = _ncFile->get_var("face_cells_col");
    _faceNodesRow = _ncFile->get_var("face_nodes_row");
    _faceNodesCol = _ncFile->get_var("face_nodes_col");


  
}

//getting values
void
NcDataReader::get_var_values()
{

     allocate_vars();

    _dimension->get( _dimensionVals, _nmesh );
    _meshID->get( _meshIDVals      , _nmesh ); 
    _facesCount->get( _facesCountVals, _nmesh );
    _cellsCount->get( _cellsCountVals, _nmesh );
    _ghostCellsCount->get( _ghostCellsCountVals, _nmesh );
    _nodesCount->get( _nodesCountVals, _nmesh );
    _mapCount->get ( _mapCountVals   , _nmesh );
    _interiorFacesGroup->get( _interiorFacesGroupVals, _nmesh );
      
     get_bndry_vals();
     get_interface_vals(); 
     get_coord_vals();
     get_connectivity_vals();

     //if ( MPI::COMM_WORLD.Get_rank() == 0) cout << _boundaryTypeVals[0] << "  " << _boundaryTypeVals[1] << endl;


}

void 
NcDataReader::allocate_vars()
{
    _dimensionVals = new  int[ _nmesh ];
    _meshIDVals    = new  int[ _nmesh    ];

    _facesCountVals = new int[ _nmesh ];
    _cellsCountVals = new int[ _nmesh ];
    _ghostCellsCountVals = new int[ _nmesh ];
    _nodesCountVals  = new int[ _nmesh ];
    _mapCountVals    = new int[ _nmesh ];
    _interiorFacesGroupVals = new int [ _nmesh ];

    _boundaryGroupVals  = new int [ _nmesh ];
    _boundarySizeVals   = new int [ _nBoun ];
    _boundaryOffsetVals = new int [ _nBoun ];
    _boundaryIDVals     = new int [ _nBoun ];
    _boundaryTypeVals.reserve(_nBoun);
    for ( int n = 0; n < _nBoun; n++)
         _boundaryTypeVals.push_back( new char[ _charSize ] );

    _interfaceGroupVals  = new int [ _nmesh  ];
    _interfaceSizeVals   = new int [ _nInterface ];
    _interfaceOffsetVals = new int [ _nInterface ];
    _interfaceIDVals     = new int [ _nInterface ];

    _xVals = new double [ _nnodes ];
    _yVals = new double [ _nnodes ];
    _zVals = new double [ _nnodes ];
 

   _faceCellsRowVals = new int [ _nfaceRow ];
   _faceCellsColVals = new int [ _nfaceCellsCol ];
   _faceNodesRowVals = new int [ _nfaceRow ];
   _faceNodesColVals = new int [ _nfaceNodesCol ];



}

//get boundary values
void 
NcDataReader::get_bndry_vals()
{

   if ( _nBoun  > 0 ){
      _boundaryGroup->get (_boundaryGroupVals, _nmesh );
      _boundarySize->get  (_boundarySizeVals,  _nBoun );
      _boundaryOffset->get( _boundaryOffsetVals, _nBoun );
      _boundaryID->get ( _boundaryIDVals, _nBoun );
      for ( int n = 0; n < _nBoun; n++){
         _boundaryType->set_cur(n);
         _boundaryType->get( _boundaryTypeVals.at(n), 1, _charSize );
      }
   }
}



void 
NcDataReader::get_interface_vals()
{
   if ( _nInterface  > 0 ){
      _interfaceGroup->get (_interfaceGroupVals, _nmesh );
      _interfaceSize->get  (_interfaceSizeVals,  _nInterface );
      _interfaceOffset->get( _interfaceOffsetVals, _nInterface );
      _interfaceID->get ( _interfaceIDVals, _nInterface );
   }


}

//get coordinates of nodes
void
NcDataReader::get_coord_vals()
{

    _x->get( _xVals, _nnodes );
    _y->get( _yVals, _nnodes );
    _z->get( _zVals, _nnodes );


}

//get coordinates of nodes
void
NcDataReader::get_connectivity_vals()
{

    _faceCellsRow->get( _faceCellsRowVals, _nfaceRow );
    _faceNodesRow->get( _faceNodesRowVals, _nfaceRow );
    _faceCellsCol->get( _faceCellsColVals, _nfaceCellsCol );
    _faceNodesCol->get( _faceNodesColVals, _nfaceNodesCol );

}


//forming MeshList
void
NcDataReader::meshList()
{

     for ( int id = 0; id < _nmesh; id++ ){

        _meshList.push_back(  new Mesh( _dimensionVals[id], _meshIDVals[id] )  );
         //storage sites
         storage_sites( id);
        //interior faces
         _meshList.at(id)->createInteriorFaceGroup( _interiorFacesGroupVals[id] );
        //boundary faces
        if ( _nBoun > 0 )
           boundary_faces( id );

        if ( _nInterface > 0 )
          interfaces( id );

        coords( id );
        face_cells( id );
        face_nodes( id );


        // mappers( id );


     }
}


void 
NcDataReader::storage_sites( int id )
{
         StorageSite& faces = _meshList.at(id)->getFaces();
         StorageSite& cells = _meshList.at(id)->getCells();
         StorageSite& nodes = _meshList.at(id)->getNodes();

         faces.setCount( _facesCountVals[id] );
         cells.setCount( _cellsCountVals[id], _ghostCellsCountVals[id] );
         nodes.setCount( _nodesCountVals[id] );
}



void
NcDataReader::boundary_faces( int id )
{
       //boundary faces
       int boun_end = _boundaryGroupVals[id];
       for ( int boun = 0; boun < boun_end; boun++){
          int bndryID = _boundaryIDVals[boun];
          int size    = _boundarySizeVals[boun];
          int offset  = _boundaryOffsetVals[boun] ;
          string boundaryType (_boundaryTypeVals.at(boun) );
         _meshList.at(id)->createBoundaryFaceGroup( size, offset, bndryID, boundaryType);
       }
 
}

void
NcDataReader::interfaces( int id )
{
         //then interface faces
     int interface_end = _interfaceGroupVals[id];
     for ( int interface = 0; interface < interface_end; interface++ ){
         int interfaceID = _interfaceIDVals[interface];
         int size        = _interfaceSizeVals[interface]; 
         int offset      = _interfaceOffsetVals[interface];
         _meshList.at(id)->createInterfaceGroup( size, offset, interfaceID );
         _meshList.at(id)->createGhostCellSite( interfaceID, shared_ptr<StorageSite>( new StorageSite(size) ) );
     }
}

void
NcDataReader::coords( int id )
{
     int nnodes = _nodesCountVals[id]; 
     int indx =  accumulate(_nodesCountVals, _nodesCountVals+id,0);
     shared_ptr< Array<Mesh::VecD3> >  coord( new Array<Mesh::VecD3>( nnodes ) );

     for ( int n = 0; n < nnodes; n++ ){
          (*coord)[n][0] = _xVals[indx];
          (*coord)[n][0] = _yVals[indx];
          (*coord)[n][0] = _zVals[indx];
          indx++;
      }

       _meshList.at(id)->setCoordinates( coord );


}

//connectivities
void
NcDataReader::face_cells( int id )
{
     //faceCells
    int nfaces = _facesCountVals[id];
     StorageSitePtr  rowSite( new StorageSite( nfaces ) );
     StorageSitePtr  colSite( new StorageSite( _cellsCountVals[id], _ghostCellsCountVals[id] ) );

     CRConnectivityPtr  faceCells ( new CRConnectivity( *rowSite, *colSite ) );

     faceCells->initCount();

     for ( int n = 0; n < nfaces; n++ ) 
         faceCells->addCount(n,2);  // two cells around a face

     faceCells->finishCount();

     int indx =  accumulate(_facesCountVals, _facesCountVals+id,0);
     for ( int n = 0; n < nfaces; n++ )
         faceCells->add( n, _faceCellsColVals[indx] );

     faceCells->finishAdd(); 

}

//connectivities
void
NcDataReader::face_nodes( int id )
{
     //faceNodes
    int nfaces = _facesCountVals[id];
     StorageSitePtr  rowSite( new StorageSite( nfaces ) );
     StorageSitePtr  colSite( new StorageSite( _nodesCountVals[id]) );

     CRConnectivityPtr  faceNodes ( new CRConnectivity( *rowSite, *colSite ) );

     faceNodes->initCount();
     int count_node = _faceNodesRowVals[1] - _faceNodesRowVals[0];   
     for ( int n = 0; n < nfaces; n++ ) 
         faceNodes->addCount(n, count_node);

     faceNodes->finishCount();

     int indx =  accumulate(_facesCountVals, _facesCountVals+id,0);
     for ( int n = 0; n < nfaces; n++ )
         faceNodes->add( n, _faceNodesColVals[indx] );

     faceNodes->finishAdd(); 

}



// void 
// NcDataReader::mappers( int id )
// {
// 
// }









