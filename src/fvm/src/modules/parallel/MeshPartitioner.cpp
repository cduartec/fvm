//
// C++ Implementation: MeshPartitioner
//
// Description: 
//
//
// Author: yildirim,,, <yildirim@cfm>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include<iostream>
#include <cassert>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "MeshPartitioner.h"
#include "Mesh.h"
#include "Array.h"
#include "CRConnectivity.h"
#include "StorageSite.h"
#include "Vector.h"
#include "OneToOneIndexMap.h"
#include <parmetis.h>
#include <utility>



MeshPartitioner::MeshPartitioner( const MeshList &mesh_list, vector<int> nPart,  vector<int> eType ):
_meshList(mesh_list), _nPart(nPart), _eType(eType), _options(0)
{
   if ( !MPI::Is_initialized() )  MPI::Init();
   init();
   assert( _meshList.size() == 1 );
}


MeshPartitioner::~MeshPartitioner()
{

    MPI::COMM_WORLD.Barrier();
    //releae memory of vector elements dynamically allocated memory
    vector< Array<int>* > ::iterator it_array;
    for ( it_array = _elemDist.begin(); it_array != _elemDist.end(); it_array++)
       delete *it_array;

    for ( it_array = _globalIndx.begin(); it_array != _globalIndx.end(); it_array++)
       delete *it_array;

   if ( !_cleanup ){
      vector< int* > ::iterator it_int;
      for ( it_int = _ePtr.begin(); it_int != _ePtr.end(); it_int++)
          delete [] *it_int;

       for ( it_int = _eInd.begin(); it_int != _eInd.end(); it_int++)
          delete [] *it_int;
	
       for ( it_int = _eElm.begin(); it_int != _eElm.end(); it_int++)
          delete [] *it_int;
	
       for ( it_int = _elmWght.begin(); it_int != _elmWght.end(); it_int++)
          delete [] *it_int;

       for ( it_int = _row.begin(); it_int != _row.end(); it_int++)
          delete [] *it_int;

       for ( it_int = _col.begin(); it_int != _col.end(); it_int++)
          delete [] *it_int;

       for ( it_int  = _elem.begin(); it_int != _elem.end(); it_int++)
          delete [] *it_int;
   }

    vector< float* > ::iterator it_float;
    for ( it_float = _tpwgts.begin(); it_float != _tpwgts.end(); it_float++)
       delete [] *it_float;

    for ( it_float = _ubvec.begin(); it_float != _ubvec.end(); it_float++)
       delete [] *it_float;
    vector< int* > ::iterator it_int;
    for ( it_int = _part.begin(); it_int != _part.end(); it_int++)
       delete [] *it_int;

 
   for ( it_int  = _elemWithGhosts.begin(); it_int != _elemWithGhosts.end(); it_int++)
       delete [] *it_int;

 

   vector< Mesh* >::iterator it_mesh;
   for ( it_mesh = _meshListLocal.begin(); it_mesh != _meshListLocal.end(); it_mesh++)
        delete *it_mesh;
  
}


void
MeshPartitioner::partition()
{

     compute_elem_dist();
     if ( _partTYPE == FIEDLER  )
        fiedler_partition();
     elem_connectivity();
     if ( _partTYPE == PARMETIS )
        parmetis_mesh();
     map_part_elms();
     count_elems_part();
     exchange_part_elems();
    //dumpTecplot();

}



void 
MeshPartitioner::mesh()
{

    CRConnectivity_cellParts();
    CRConnectivity_faceParts();
    interfaces();
    faceCells_faceNodes();
    non_interior_cells();
    order_faceCells_faceNodes();
    coordinates();
    exchange_interface_meshes();
    mesh_setup();
    mappers();

}






void 
MeshPartitioner::dumpTecplot()
{

    //just for mesh 0;
    MPI::COMM_WORLD.Barrier();

    const Mesh& mesh = *(_meshList.at(0));
    const Array<Mesh::VecD3>&  coord = mesh.getNodeCoordinates();

    const StorageSite&   cellSite   = _meshList.at(0)->getCells();
    int tot_elems = cellSite.getSelfCount();
    const CRConnectivity& cellNodes = _meshList.at(0)->getCellNodes();
    //connectivity
    if ( _procID == 0 )
       for ( int n = 0; n < tot_elems; n++ ){
          int nnodes =  cellNodes.getCount(n);
         cout << " elem = " << n << ",   ";
          for ( int node = 0; node < nnodes; node++ )
                 cout << cellNodes(n,node) << "      ";
          cout << endl;
       }

    int tot_nodes = coord.getLength();

    if ( _procID == 0 )
        for ( int node = 0; node < tot_nodes; node++)
            cout << " nodeID = " << node << " x = " << coord[node][0] <<  " y = " << coord[node][1] << " z = " << coord[node][2] << endl;


   

    if ( _procID == 0 ) {
       MPI::Status status_row;
       MPI::Status status_col;
       int tag_row = 9;
       int tag_col = 99;
       int tot_elems = _totElems.at(0);
       ofstream part_file( "partiton.dat" );
       
       int *row = new int[tot_elems];  //open big array
       int *col = new int[8*tot_elems]; //maximum hexa 

      
        part_file << "title = \" tecplot file for partionoing \" "  << endl;
        part_file << "variables = \"x\",  \"y\", \"z\", \"partID\" " << endl;
        part_file << "zone N = " << tot_nodes << " E = " << tot_elems <<
           " DATAPACKING = BLOCK,  VARLOCATION = ([4]=CELLCENTERED), ZONETYPE=FETRIANGLE " << endl;

        //x 
        for ( int n = 0; n < tot_nodes; n++){
           part_file << scientific  << coord[n][0] << "     " ;
           if ( n % 5 == 0 ) part_file << endl;
        }
        part_file << endl;
        //y
        for ( int n= 0; n < tot_nodes; n++){
           part_file << scientific << coord[n][1] << "     ";
           if ( n % 5 == 0 ) part_file << endl;
        }
        part_file << endl;
        //z
        for ( int n = 0; n < tot_nodes; n++){
           part_file << scientific << coord[n][2] << "     ";
           if ( n % 5 == 0) part_file << endl;
        }
        part_file <<  endl;

      for ( int elem = 0; elem < _nelems.at(0);  elem++){
               part_file << 0  << "     ";
               if (  elem % 10 == 0 ) part_file << endl;
       }

       vector< shared_ptr< Array<int> > >  row_root;
       vector< shared_ptr< Array<int> > >  col_root;

       //copy row and col values from root
       row_root.push_back( shared_ptr< Array<int> > (new Array<int> (_nelems.at(0)+1) ) );
       col_root.push_back( shared_ptr< Array<int> > (new Array<int> (_colDim.at(0)  ) ) );

       for ( int r = 0; r < _nelems.at(0)+1; r++)
          (*row_root.at(0))[r] = _row.at(0)[r];

       for ( int c = 0; c < _colDim.at(0); c++)
          (*col_root.at(0))[c] = _col.at(0)[c];


       for ( int p = 1; p < _nPart.at(0); p++){
           MPI::COMM_WORLD.Recv( row, tot_elems, MPI::INT, p, tag_row, status_row);
           MPI::COMM_WORLD.Recv( col, 8*tot_elems, MPI::INT, p, tag_col, status_col);

           int nelems = status_row.Get_count(MPI::INT) - 1;
           int col_dim = status_col.Get_count(MPI::INT);
           row_root.push_back( shared_ptr< Array<int> > (new Array<int> (nelems+1) ) );
           col_root.push_back( shared_ptr< Array<int> > (new Array<int> (col_dim)  ) );
           //fill row_root
           for ( int r = 0; r < nelems+1; r++)
              (*row_root.at(p))[r] = row[r];
           //fill col_root
           for ( int c = 0; c < col_dim; c++)
              (*col_root.at(p))[c] = col[c];

           for ( int elem = 0; elem < nelems;  elem++){
               part_file << p  << "     ";
               if (  elem % 10 == 0 ) part_file << endl;
           }
       }
      part_file << endl;


      for ( int p = 0; p < _nPart.at(0); p++){
         //connectivity
        int nelems = row_root.at(p)->getLength() - 1;
         for ( int elem = 0; elem < nelems;  elem++){
             int node_start = (*row_root.at(p))[elem];
             int node_end   = (*row_root.at(p))[elem+1];
             for ( int node = node_start; node < node_end; node++)
                 part_file << setw(6) <<  (*col_root.at(p))[node]+1 << "     ";
              part_file <<endl;
         }
       }

       part_file.close();
       delete [] row;
       delete [] col;

    } else { 
       int tag_row = 9;
       int tag_col = 99;

        MPI::COMM_WORLD.Send(_row.at(0), _nelems.at(0)+1, MPI::INT, 0, tag_row);
        MPI::COMM_WORLD.Send(_col.at(0), _colDim.at(0)  , MPI::INT, 0, tag_col);

    }

}


             //SET PROPERTIES METHODS
void 
MeshPartitioner::setWeightType( int weight_type )
{
   for ( int id = 0; id < _nmesh; id++)
      _wghtFlag.at(id) =  weight_type;

}

void 
MeshPartitioner::setNumFlag( int num_flag )
{
  for ( int id = 0; id < _nmesh; id++)
      _numFlag.at(id) = num_flag;

}

           // PRIVATE METHODS

void
MeshPartitioner::init()
{
    _partTYPE  = PARMETIS;
    _procID    = MPI::COMM_WORLD.Get_rank();
    _nmesh     = _meshList.size();

   _totElems.resize  ( _nmesh );
   _totElemsAndGhosts.resize( _nmesh );
   _wghtFlag.resize( _nmesh );
   _numFlag.resize ( _nmesh );
   _ncon.resize    ( _nmesh ); 
   _ncommonNodes.resize( _nmesh );
   _mapPartAndElms.resize( _nmesh );
   _boundarySet.resize( _nmesh );
   _interfaceSet.resize( _nmesh );
   _mapBounIDAndCell.resize( _nmesh );
   _mapBounIDAndBounType.resize( _nmesh );
   _nelems.resize( _nmesh );
   _nelemsWithGhosts.resize( _nmesh );
   _colDim.resize( _nmesh );
   _edgecut.resize( _nmesh );
   _elemSet.resize(_nmesh);
   _nonInteriorCells.resize(_nmesh ); //local numbering
   _bndryOffsets.resize( _nmesh );
   _interfaceOffsets.resize( _nmesh );
   _cellToOrderedCell.resize( _nmesh );
   _globalToLocalMappers.resize( _nmesh );
   _localToGlobalMappers.resize( _nmesh );
   _windowSize.resize( _nmesh );
   _fromIndices.resize( _nmesh );
   _toIndices.resize( _nmesh );
   _cleanup = false;
   _debugMode = false;
    for ( int id = 0; id < _nmesh; id++){
        StorageSite& site = _meshList[id]->getCells();
       _totElems.at(id)   = site.getSelfCount();
       _totElemsAndGhosts.at(id) = site.getCount();
       _wghtFlag.at(id) = int( NOWEIGHTS ); //No Weights : default value
       _numFlag.at(id)  = int( C_STYLE );   //C Style numbering :: default_value
       _ncon.at(id)     = 2;               //number of specified weights : default value for contigous domain ncon > 1
       //if it is assemble mesh ncon will be equal to num of assembled mesh
       if (  _meshList.at(id)->isMergedMesh() )
           _ncon.at(id)     = _meshList.at(id)->getNumOfAssembleMesh();

       //assign ubvec
       _ubvec.push_back( new float[_ncon.at(id) ] );
       for ( int n = 0; n < _ncon.at(id); n++)
         _ubvec.at(id)[n] = 1.05f; //1.05 suggested value from parMetis manual


        //assign elementy type
        switch (_eType.at(id) ){
            case   TRI :
                   _ncommonNodes.at(id) = 2;
                   break;
            case   TETRA :
                   _ncommonNodes.at(id) = 3;
                   break;
            case   HEXA :
                   _ncommonNodes.at(id) = 4;
                   break;
            case   QUAD :
                   _ncommonNodes.at(id) = 2;
                    break;
            default :
                    cout << " ONLY TRIANGLE, TETRAHEDRAL, HEXAHEDRAL and QUADRILATERAL elements must be chose " <<
                    endl;     abort(); 
        } 

        //get tpwgts
        int ncon_by_nparts = _ncon.at(id) * _nPart.at(id);
        _tpwgts.push_back( new float[ ncon_by_nparts ] );
        for (int n = 0; n < ncon_by_nparts; n++){
            _tpwgts.at(id)[n] = 1.0f / float( ncon_by_nparts );
        }

       //edgecut
        _edgecut.at(id) = -1;

      _interfaceMeshCounts.push_back (  ArrayIntPtr( new Array<int>(_nPart.at(id)) )  );
      _procTotalInterfaces.push_back (  ArrayIntPtr( new Array<int>(_nPart.at(id)) )  );

    }


}



void 
MeshPartitioner::compute_elem_dist()
{

   for (int id = 0; id < _nmesh; id++){
      int nelems = _totElems[id];
      int npart  = _nPart[id];
      int nremainder = nelems % npart;
      int init_dist = (nelems - nremainder) / npart;
      _elemDist.push_back( new Array<int>( npart) );
      _globalIndx.push_back( new Array<int>(npart+1) );
      *_elemDist.at(id) = init_dist;

       int p = 0;
       while ( nremainder != 0 ){
           (*_elemDist.at(id))[p % npart]++;
            nremainder--;
            p++;
       }

       (*_globalIndx[id]) = 0;
       int sum_elem = 0;
       for ( int n = 1; n <= npart; n++){
           sum_elem += (*_elemDist.at(id))[n-1];
           ((*_globalIndx.at(id))[n]) =  sum_elem;
       }
   }

   for (int id = 0; id < _nmesh; id++){
       int mesh_nlocal = (*_elemDist.at(id))[_procID];
     _part.push_back( new  int[mesh_nlocal] );
      for ( int n = 0; n < mesh_nlocal; n++)
         _part.at(id)[n] = -1;
   }

   if ( _debugMode ) 
      DEBUG_compute_elem_dist();

}


//debug compute_elem_dist() function
void 
MeshPartitioner::DEBUG_compute_elem_dist()
{
   //open file
   debug_file_open("compute_elem_dist");
  //_totElms
  _debugFile << "_totElems = " << _totElems[0] << endl;
  _debugFile << endl;
  //_npart
  _debugFile << "_npart    = " << _nPart[0]    << endl;
  _debugFile << endl;
  //_elemDist
  _debugFile << "_elemDist : " << endl;
  _debugFile << endl;
  for( int n = 0; n < _nPart[0]; n++ )
     _debugFile << "_elemDist[" << n << "] = " << (*_elemDist[0])[n] << endl;  
  _debugFile << endl;
  //_globalIndx
  _debugFile << "_globalIndx : " << endl;
  for( int n = 0; n < _nPart[0]+1; n++ )
     _debugFile << "_globalIndx[" << n << "] = " << (*_globalIndx[0])[n] << endl;  
  _debugFile << endl;
  //close file
   debug_file_close();

}




void
MeshPartitioner::elem_connectivity()
{

   for (int id = 0; id < _nmesh; id++){
       //allocate local ePtr for parMetis
       int mesh_nlocal = (*_elemDist.at(id))[_procID];
      _ePtr.push_back( new int[mesh_nlocal+1] );
      _eElm.push_back( new int[mesh_nlocal+1] );
       //element weights  
      _elmWght.push_back( new int[_ncon.at(id)*mesh_nlocal] );
       if (  !_meshList.at(id)->isMergedMesh() ){
          _wghtFlag.at(id) = int( NOWEIGHTS ); //No Weights : default value
          for ( int n = 0; n < _ncon.at(id)*mesh_nlocal; n++)
             _elmWght.at(id)[n] = 1;
       } else {
          _wghtFlag.at(id) = int( WEIGTHS_ONLY_VERTICES ); 
           const Array<int>& cellColors = _meshList.at(id)->getCellColors();
           int indx = 0 ;
           for ( int n =0; n < mesh_nlocal; n++ ){
              for ( int i = 0; i < _meshList.at(id)->getNumOfAssembleMesh(); i++ ){
                 _elmWght.at(id)[indx] = 0;
                  if ( cellColors[ (*_globalIndx[id])[_procID] + n ] == i )
                      _elmWght.at(id)[indx] = 1;
                  indx++;
              }
           }
       }
       //allocate local eInd for ParMETIS
      _eInd.push_back( new int[get_local_nodes(id)] );

//       //setting ePtr and eInd for ParMETIS
      set_eptr_eind(id);
   }

   if ( _debugMode )
      DEBUG_elem_connectivity();

}

int
MeshPartitioner::get_local_nodes( int id )
{
      const Mesh* mesh = _meshList[id];
      const CRConnectivity& cellNodes = mesh->getCellNodes();

      //get local nodes
      int local_nodes = 0;
      int nstart = (*_globalIndx.at(id))[_procID];
      int npart  = nstart + (*_elemDist[id])[_procID];
      for ( int n = nstart; n < npart; n++)
           local_nodes += cellNodes.getCount(n);

       return local_nodes;
}

void
MeshPartitioner::set_eptr_eind( int id )
{
      const Mesh* mesh = _meshList[id];
      const CRConnectivity& cellNodes = mesh->getCellNodes();

      int elem_start   = (*_globalIndx.at(id))[_procID];
      int elem_finish  = elem_start + (*_elemDist[id])[_procID];
      int indxInd   = 0;
      int indxPtr   = 0;
      _ePtr.at(id)[indxPtr] = 0;
      for ( int elem = elem_start; elem < elem_finish; elem++ ){
          _eElm.at(id)[indxPtr] = elem;  //mapping local to global Index
           indxPtr++;
          _ePtr.at(id)[indxPtr] = _ePtr.at(id)[indxPtr-1] +  cellNodes.getCount(elem);
          if ( _eType.at(id) == TRI || _eType.at(id) == TETRA || _eType.at(id) == HEXA ){ // connectivity orientation is not important
              for (  int node = 0; node < cellNodes.getCount(elem); node++ )
                 _eInd.at(id)[indxInd++] = cellNodes(elem,node);
          }

         if ( _eType.at(id) == QUAD ) {  //connectivity orientation is reversed for QUADs since Parmetis require clockwise orientation
              for (  int node = cellNodes.getCount(elem)-1; node >=0; node-- )
                 _eInd.at(id)[indxInd++] = cellNodes(elem,node);
         }
  
      }

}

void
MeshPartitioner::DEBUG_elem_connectivity()
{
    debug_file_open("elem_connectivity");
    //ePtr
    _debugFile << " _ePtr :" << endl;
    _debugFile << endl;
    int mesh_nlocal = (*_elemDist.at(0))[_procID];
     for ( int i = 0; i <= mesh_nlocal; i++ ){
         _debugFile << " _ePtr[" << i << "] = " << _ePtr[0][i] << endl;
     }
    _debugFile << endl;
    //eInd
    _debugFile << "_eInd : " << endl;
    _debugFile << endl;
    for ( int i = 0; i < mesh_nlocal; i++ ){
       _debugFile << "_eInd[" << i << "], glblCellID =  " << setw(3) << _eElm.at(0)[i] << ",  ";
       for ( int j = _ePtr[0][i]; j < _ePtr[0][i+1]; j++ ){
           _debugFile << setw(5) <<  _eInd.at(0)[j] << "   ";
       }
       _debugFile << endl;
    }
    _debugFile << endl;
    debug_file_close();
}


void
MeshPartitioner::parmetis_mesh()
{
   MPI_Comm comm_world = MPI::COMM_WORLD;
   for ( int id = 0; id < _nmesh; id++){
       ParMETIS_V3_PartMeshKway( &(*_globalIndx.at(id))[0], _ePtr.at(id), _eInd.at(id),
        _elmWght.at(id), &_wghtFlag.at(id), &_numFlag.at(id), &_ncon.at(id),  &_ncommonNodes.at(id),
        &_nPart.at(id), _tpwgts.at(id), _ubvec.at(id), &_options, &_edgecut.at(id), _part.at(id), &comm_world );

   } 

   if ( _debugMode )
      DEBUG_parmetis_mesh();

}

//debuggin parmetis_mesh
void
MeshPartitioner::DEBUG_parmetis_mesh()
{

   debug_file_open("parmetis_mesh");
    //_part
   _debugFile << "_part :" << endl;
   _debugFile << endl; 
   int elem_start   = (*_globalIndx.at(0))[_procID];
   int elem_finish  = elem_start + (*_elemDist[0])[_procID];
   int indx = 0;
   for ( int i = elem_start; i < elem_finish; i++ ){
      _debugFile << "_part[" << indx << "] = " << _part.at(0)[indx] << endl;
      indx++;
   }
   _debugFile << endl;  
   debug_file_close();

}


void
MeshPartitioner::fiedler_order( const string& fname )
{
   _partTYPE = FIEDLER;
  //read permutation file
   int totElms          = _totElems[0];
   int totElmsAndGhost = _totElemsAndGhosts[0];
   _fiedlerMap = ArrayIntPtr( new Array<int>(totElmsAndGhost) );
   ifstream  permutation_file( fname.c_str() );
   int cellID  = -1;
   for ( int i = 0; i < totElms; i++ ){
        permutation_file >> cellID;
        (*_fiedlerMap)[i] = cellID - 1; //permutation 
   }
   for ( int i = totElms; i < totElmsAndGhost; i++ ){
        (*_fiedlerMap)[i] = i; //permutation 
   }

   permutation_file.close();
}

void
MeshPartitioner::fiedler_partition()
{
   MPI::COMM_WORLD.Barrier();

   //copy to part
   int elem_start   = (*_globalIndx.at(0))[_procID];
   int elem_finish  = elem_start + (*_elemDist[0])[_procID];

   int indx = 0;
   for ( int i = elem_start; i < elem_finish; i++ ){
      _part.at(0)[indx] = _procID; 
      indx++;
   }

   CRConnectivity&  faceCells = _meshList[0]->getAllFaceCells();
   faceCells.reorder( *_fiedlerMap );

   MPI::COMM_WORLD.Barrier();

   if ( _debugMode )
      DEBUG_fiedler_partition();

}  

//debuggin fiedler partition
void
MeshPartitioner::DEBUG_fiedler_partition()
{
   debug_file_open("fiedler_partition");
   //_fieldermap
   _debugFile << "Fiedler Map :" << endl;
   _debugFile << endl;
    int totElms = _totElems[0];
    for ( int n = 0; n < totElms; n++ )
       _debugFile << "_fiedlerMap[" << n << "] = " <<  (*_fiedlerMap)[n] << endl;
    _debugFile << endl; 
    //_part
   _debugFile << "_part :" << endl;
   _debugFile << endl; 
   int elem_start   = (*_globalIndx.at(0))[_procID];
   int elem_finish  = elem_start + (*_elemDist[0])[_procID];
   int indx = 0;
   for ( int i = elem_start; i < elem_finish; i++ ){
      _debugFile << "_part[" << indx << "] = " << _part.at(0)[indx] << endl;
      indx++;
   }
   _debugFile << endl;  
   //faceCells after Fiedler
   CRConnectivity&  faceCells = _meshList[0]->getAllFaceCells();
   CRConnectivityPrintFile( faceCells, "faceCells" );
   debug_file_close();

}

//this store local partID and local elm number mappings (global elem number can be deduct from _eElm data structure(eElem[local] = globalID])
void 
MeshPartitioner::map_part_elms()
{
   for (int  id = 0; id < _nmesh; id++){
      int nlocal_elem = (*_elemDist.at(id))[_procID];
      for ( int elm = 0; elm < nlocal_elem; elm++){
         int partID = _part[id][elm];
         _mapPartAndElms.at(id).insert(pair<int,int>(partID,elm));
      }
  
   }

   if ( _debugMode )
      DEBUG_map_part_elms();
}

void
MeshPartitioner::DEBUG_map_part_elms()
{
   //open file
   debug_file_open("map_part_elms");
   //header
   _debugFile << " _mapPartAndElms : " << endl;
   _debugFile << endl;
   for ( int p = 0; p < _nPart[0]; p++){
       multimap<int,int>::iterator it, itlow, itup;
       itlow = _mapPartAndElms.at(0).lower_bound(p);
       itup  = _mapPartAndElms.at(0).upper_bound(p);
       for( it = itlow; it != itup; it++)
          _debugFile << " partID = " << it->first << " elemID = " << it->second << endl;
   }
   _debugFile  << endl;
   //close file
   debug_file_close();

}
//compute _nelems and _colDim  for each partition
void 
MeshPartitioner::count_elems_part()
{
   for ( int id = 0; id < _nmesh; id++){
       for ( int partID = 0; partID < _nPart.at(id); partID++){
          int nelems_local = _mapPartAndElms.at(id).count(partID);

          //sum to find size of ncol
          multimap<int,int>::const_iterator it = _mapPartAndElms.at(id).find(partID);
          multimap<int,int>::const_iterator itlow = _mapPartAndElms.at(id).lower_bound(partID);
          multimap<int,int>::const_iterator itup = _mapPartAndElms.at(id).upper_bound(partID);
          int ncol_local = 0;
          for ( it = itlow; it != itup; it++){
              int pos = it->second;  // element number
              ncol_local += _ePtr.at(id)[pos+1] - _ePtr.at(id)[pos];
          }

          MPI::COMM_WORLD.Reduce(&nelems_local, &_nelems.at(id), 1, MPI::INT, MPI::SUM, partID);
          MPI::COMM_WORLD.Reduce(&ncol_local, &_colDim.at(id), 1, MPI::INT, MPI::SUM, partID);

       }
       //now each processor now how many elements and nodes
      _row.push_back ( new int[_nelems.at(id)+1] );
      _elem.push_back( new int[_nelems.at(id) ] );
      _col.push_back ( new int[_colDim.at(id)]   );
       for ( int n = 0; n < _nelems.at(id)+1; n++ )
            _row.at(id)[n] = -1;
       for ( int n = 0; n < _colDim.at(id); n++)
            _col.at(id)[n] = -1;
       for ( int n = 0; n < _nelems.at(id); n++)
            _elem.at(id)[n] = -1;

   }

   if ( _debugMode ) 
      DEBUG_count_elems_part();

}

//debug count_elems_part
void
MeshPartitioner::DEBUG_count_elems_part()
{
  //open debug file
  debug_file_open("count_elems_part");
  //_nelems
  _debugFile << "_nelems = " << _nelems.at(0) << endl;
  _debugFile << endl;
  //_colDim
  _debugFile << "_colDim = " << _colDim.at(0) << endl;
  //close debug file
  debug_file_close();



}



void
MeshPartitioner::exchange_part_elems()
{

   for ( int id = 0; id < _nmesh; id++){

       int *countsRow  =  new int[_nPart.at(id)];
       int *countsCol  =  new int[_nPart.at(id)];
       int *offsetsRow =  new int[_nPart.at(id)];
       int *offsetsCol =  new int[_nPart.at(id)];


       for ( int partID = 0; partID < _nPart.at(id); partID++){
          int nelems_local = _mapPartAndElms.at(id).count(partID);
          int *row_local  = new int[nelems_local];
          int *elem_local = new int[nelems_local];

           multimap<int,int>::const_iterator it = _mapPartAndElms.at(id).find(partID);
           multimap<int,int>::const_iterator itlow = _mapPartAndElms.at(id).lower_bound(partID);
           multimap<int,int>::const_iterator itup = _mapPartAndElms.at(id).upper_bound(partID);

           //fill row array
           int indx = 0;
           int ncol_local = 0; 
           for ( it = itlow; it != itup; it++){
              int pos         = it->second;  // element number
              ncol_local     += _ePtr.at(id)[pos+1] - _ePtr.at(id)[pos];
              row_local[indx]  = _ePtr.at(id)[pos+1]-_ePtr.at(id)[pos]; //aggregation before shipping
              elem_local[indx] = _eElm.at(id)[pos];  //globalID stored in _eElm
              indx++;
           }

           //fill col array
           int *col_local = new int[ncol_local];
           indx = 0;
           for ( it = itlow; it != itup; it++ ){
              int elID = it->second;
              int node_start = _ePtr.at(id)[elID];
              int node_end   = _ePtr.at(id)[elID+1];
              for ( int node = node_start; node < node_end; node++){
                 col_local[indx++] = _eInd.at(id)[node];
              }
           }

         //forming counts
         int nrow_local = nelems_local;
         MPI::COMM_WORLD.Allgather(&nrow_local, 1, MPI::INT, countsRow, 1, MPI::INT);
         MPI::COMM_WORLD.Allgather(&ncol_local, 1, MPI::INT, countsCol, 1, MPI::INT);


        //form offsets
        offsetsRow[0]  = 0;
        offsetsCol[0]  = 0;
        for ( int p = 1; p < int(_nPart.at(id)); p++ ){
           offsetsRow[p]  = countsRow[p-1]  + offsetsRow[p-1];
           offsetsCol[p]  = countsCol[p-1]  + offsetsCol[p-1];
        } 

        //gathering partial partions for _row and _col
        MPI::COMM_WORLD.Gatherv(row_local, countsRow[_procID], MPI::INT, _row.at(id), 
                                countsRow, offsetsRow, MPI::INT, partID);

        MPI::COMM_WORLD.Gatherv(col_local, countsCol[_procID], MPI::INT, _col.at(id), 
                                countsCol, offsetsCol, MPI::INT, partID);
 
        MPI::COMM_WORLD.Gatherv(elem_local, countsRow[_procID], MPI::INT, _elem.at(id),
                                countsRow, offsetsRow, MPI::INT, partID);


        delete [] row_local;
        delete [] col_local;
        delete [] elem_local;

       }  // for::partID

       delete [] countsRow ;
       delete [] countsCol ;
       delete [] offsetsRow;
       delete [] offsetsCol;

    }  // for::meshID


   shift_sum_row();

   //clean up 
   if ( _cleanup )
      cleanup_follow_exchange_part_elems();

   if ( _debugMode )
      DEBUG_exchange_part_elems();

}

void
MeshPartitioner::shift_sum_row()
{
   for ( int id = 0; id < _nmesh; id++){
       //shift [0,n] to [1,n+1]
       for ( int n = _nelems.at(id); n > 0; n--)
           _row.at(id)[n] = _row.at(id)[n-1];
      _row.at(id)[0] = 0;
      //summing row ex: row = {0,3,6,9,...} for triangle (three nodes)
       for ( int n = 1; n < _nelems.at(id)+1; n++ )
          _row.at(id)[n] += _row.at(id)[n-1];

   }


}


void 
MeshPartitioner::cleanup_follow_exchange_part_elems()
{
    //dont release memory until all process reach this point
    MPI::COMM_WORLD.Barrier();
    vector< int* > ::iterator it_int;
    for ( it_int = _ePtr.begin(); it_int != _ePtr.end(); it_int++)
       delete [] *it_int;

    for ( it_int = _eInd.begin(); it_int != _eInd.end(); it_int++)
       delete [] *it_int;

    for ( it_int = _eElm.begin(); it_int != _eElm.end(); it_int++)
       delete [] *it_int;

    for ( it_int = _elmWght.begin(); it_int != _elmWght.end(); it_int++)
       delete [] *it_int;

    for ( it_int = _row.begin(); it_int != _row.end(); it_int++)
       delete [] *it_int;

    for ( it_int = _col.begin(); it_int != _col.end(); it_int++)
       delete [] *it_int;


}

void
MeshPartitioner::DEBUG_exchange_part_elems()
{
     debug_file_open("exchange_part_elems");
     //_row 
     for ( int n = 0; n < _nelems.at(0)+1; n++){
         _debugFile << " _row[" << n << "] = " << _row.at(0)[n] << endl;
     }
     _debugFile << endl;
     //_col
     for ( int n = 0; n < _colDim.at(0); n++){
        _debugFile << " _col[" << n << "] = " << _col.at(0)[n] << endl;
     }
     _debugFile << endl;
     //elem 
     for ( int n = 0; n < _nelems.at(0); n++){
         _debugFile << " _elem[" << n << "] = " << _elem.at(0)[n] << endl;
     }
     _debugFile << endl;
     debug_file_close();

}



void 
MeshPartitioner::mesh_setup()
{
    for ( int id = 0; id < _nmesh; id++){
        //interior faces
        _meshListLocal.at(id)->createInteriorFaceGroup( count_interior_faces(id) );

        //boundary faces
        set<int>::const_iterator it_set;
        for ( it_set = _boundarySet.at(id).begin(); it_set != _boundarySet.at(id).end(); it_set++){
           int bndryID = *it_set;
           int size   =  _mapBounIDAndCell.at(id).count(bndryID);
           if ( size > 0 ){
              int offset =  _bndryOffsets.at(id)[ bndryID ] ;
              string boundaryType = _mapBounIDAndBounType.at(id)[bndryID];
              _meshListLocal.at(id)->createBoundaryFaceGroup(size, offset, bndryID, boundaryType);
           }
         }

        //then interface faces
        for ( it_set = _interfaceSet.at(id).begin(); it_set != _interfaceSet.at(id).end(); it_set++){
           int interfaceID = *it_set;
           int size = int(_interfaceMap.at(id).count( interfaceID ) );
           int offset = _interfaceOffsets.at(id)[ interfaceID ];
           _meshListLocal.at(id)->createInterfaceGroup( size, offset, interfaceID );
            shared_ptr<StorageSite> siteGather ( new StorageSite(size) );
            shared_ptr<StorageSite> siteScatter( new StorageSite(size) );
            siteGather->setScatterProcID( _procID );
            siteGather->setGatherProcID ( interfaceID );
            siteScatter->setScatterProcID( _procID );
            siteScatter->setGatherProcID ( interfaceID );
            int packed_info = (std::max(_procID,interfaceID) << 16 ) | ( std::min(_procID,interfaceID) );
            siteScatter->setTag( packed_info );	


            Mesh::PartIDMeshIDPair  pairID = make_pair<int,int>(interfaceID, id);
           _meshListLocal.at(id)->createGhostCellSiteScatter( pairID, siteScatter );
           _meshListLocal.at(id)->createGhostCellSiteGather ( pairID, siteScatter );

         }

       _meshListLocal.at(id)->setCoordinates( _coord.at(id)            );
       _meshListLocal.at(id)->setFaceNodes  ( _faceNodesOrdered.at(id) );
       _meshListLocal.at(id)->setFaceCells  ( _faceCellsOrdered.at(id) );
        if (  _meshList.at(id)->isMergedMesh() ){
            setMeshColors();
        }

    }


}

//if it is merged, we want to fill color array in Mesh class for further usage
void
MeshPartitioner::setMeshColors()
{
    //get number of meshes assembled from meshList
    int nmesh = _meshList.at(0)->getNumOfAssembleMesh();
    //assing nmesh and make Mesh::_isAssembledMesh == true
    _meshListLocal.at(0)->setNumOfAssembleMesh( nmesh );
    _meshListLocal.at(0)->createCellColor();
    //get cellsite storagesite
    const StorageSite& cellSite = _meshListLocal.at(0)->getCells();
    Array<int>&   colorGlbl     = _meshList.at(0)->getCellColors();
    Array<int>&   colorLocal      = _meshListLocal.at(0)->getCellColors();
    Array<int>&   colorOtherLocal = _meshListLocal.at(0)->getCellColorsOther();
    const  map<int,int>&  localToGlobalMappers = _localToGlobalMappers.at(0);
    //loop first over inner cells to color them
    for ( int i = 0; i <  cellSite.getSelfCount(); i++ ){
        int glblID = localToGlobalMappers.find(i)->second;
        colorLocal[i] = colorGlbl[ glblID ];
    }
    //coloring other cells (boundary+ghostcells)
    //we check across cell's color, they should have the same color.
    const CRConnectivity& cellCells = _meshListLocal.at(0)->getCellCells();
    for ( int i = cellSite.getSelfCount(); i < cellSite.getCount(); i++ ){
       int acrossCellID = cellCells(i,0); //ghost or boundary has only one cell connected 
       colorLocal[i] = colorLocal[ acrossCellID ];
    }

    //colorOtherLocal loop first over inner cells to color them
    for ( int i = 0; i <  cellSite.getCount(); i++ ){
        int glblID = localToGlobalMappers.find(i)->second;
        colorOtherLocal[i] = colorGlbl[ glblID ];
    }


}



//get boundary information for process
void
MeshPartitioner::mapBounIDAndCell(int id)
{

   //mapBounIDAndCell  store global information
   //_mapBounIDAndCell store  local (process) information
    multimap<int,int>  mapBounIDAndCell;

    //boundary information has been stored
    const FaceGroupList&  boundaryFaceGroups = _meshList.at(id)->getBoundaryFaceGroups();
    int indx = _totElems.at(id);

    for ( int bounID = 0; bounID < int(boundaryFaceGroups.size()); bounID++){
        int group_id = boundaryFaceGroups.at(bounID)->id;
        _boundarySet.at(id).insert( group_id );
        string boun_type( boundaryFaceGroups.at(bounID)->groupType );
 
        _mapBounIDAndBounType.at(id).insert( pair<int,string>(group_id, boun_type) );

        int nBounElm = boundaryFaceGroups.at(bounID)->site.getCount();

        for ( int n = 0; n < nBounElm; n++){
           mapBounIDAndCell.insert( pair<int,int>(group_id, indx) );
           indx++;
        }
     }

     //putting local elements  in set to check fast way 
     for ( int n = 0; n < _nelems.at(id); n++ )
        _elemSet.at(id).insert( _elem.at(id)[n] );

     multimap<int,int>::const_iterator  it;
     const CRConnectivity& cellCells = _meshList.at(id)->getCellCells();
     for ( it = mapBounIDAndCell.begin(); it != mapBounIDAndCell.end(); it++ ){
        int boun_cell_id = it->second;
        int neigh_id     = cellCells(boun_cell_id,0); //assuming just one neighbour for boundary
        if ( _elemSet.at(id).count( neigh_id ) > 0 )
            _mapBounIDAndCell.at(id).insert( pair<int,int>(it->first, it->second) );
     }

     if ( _debugMode )
        DEBUG_mapBounIDAndCell();

}

void 
MeshPartitioner::DEBUG_mapBounIDAndCell()
{
    //open file
    debug_file_open("mapBounIDAndCell");
     //_elemset
     _debugFile << "_boundarySet : " << endl;
     _debugFile << endl;
     foreach(  const set<int>::value_type id ,_boundarySet.at(0) ){
         _debugFile <<  id << endl;
     }
     _debugFile << endl;
     //dump boundary names
     _debugFile << "_mapBounIDAndBounType : " << endl;
     _debugFile << endl;
     multimap<int,string>::iterator it_multimapS;
     for ( it_multimapS  = _mapBounIDAndBounType.at(0).begin(); 
           it_multimapS != _mapBounIDAndBounType.at(0).end(); it_multimapS++)
          _debugFile << "Boundary multimap = " << it_multimapS->first << "    " << it_multimapS->second << endl;
     _debugFile << endl;
     //_elemset
     _debugFile << "_elemSet : " << endl;
     _debugFile << endl;
     foreach(  const set<int>::value_type cellID ,_elemSet.at(0) ){
         _debugFile <<  cellID << endl;
     }
     _debugFile << endl;
    //boundaryID to Cell
    multimap<int,int>::iterator it_multimap;
    for ( it_multimap  = _mapBounIDAndCell.at(0).begin(); 
          it_multimap != _mapBounIDAndCell.at(0).end(); it_multimap++)
    _debugFile << "Boundary multimap = " << it_multimap->first << "    " << it_multimap->second << endl;
    _debugFile << endl;

    debug_file_close();
}

//adding ghost cell elements to local elements
void
MeshPartitioner::resize_elem(int id)
{
   int  tot_cells = _mapBounIDAndCell.at(id).size() + _nelems.at(id);
   _nelemsWithGhosts.at(id) = tot_cells;
   _elemWithGhosts.push_back(  new int[ tot_cells] );
 
   //assign old values
   for ( int n = 0;  n < _nelems.at(id); n++)
       _elemWithGhosts.at(id)[n] = _elem.at(id)[n];
   //ghost part assigned
   multimap<int,int>::const_iterator it;
   int indx = _nelems.at(id);
   for ( it = _mapBounIDAndCell.at(id).begin(); it != _mapBounIDAndCell.at(id).end(); it++){
      _elemWithGhosts.at(id)[indx] = it->second;
      indx++;
   }
   
   if ( _debugMode )
      DEBUG_resize_elem();

}

void 
MeshPartitioner::DEBUG_resize_elem()
{
   //open file
   debug_file_open("resize_elem");
   //_nelemsWithGhosts
   _debugFile << "_nelemsWithGhosts : " << _nelemsWithGhosts.at(0) << endl;
   _debugFile << endl;
    //_elemWithGhosts
   _debugFile << "_elemWithGhosts : " << endl;
   _debugFile << endl;
    for ( int n = 0; n < _nelemsWithGhosts.at(0); n++ )
       _debugFile << _elemWithGhosts.at(0)[n] << endl;
   //close file
   debug_file_close();

}


//construct CRConnectivity cellParts
void
MeshPartitioner::CRConnectivity_cellParts()
{
    vector< int* >  elemGlobal;
    vector< int* >  distGlobal;  //total partition + 1 suche that 0, 5, 10, 15 

    for ( int id = 0; id < _nmesh; id++){
        mapBounIDAndCell(id);
        resize_elem(id);

        elemGlobal.push_back( new int [_totElemsAndGhosts.at(id) ]   ); //global array to aggregation
        distGlobal.push_back( new int [ _nPart.at(id) + 1 ] );
        int *offsets = new int [ _nPart.at(id) ];


        MPI::COMM_WORLD.Allgather(&_nelemsWithGhosts.at(id), 1, MPI::INT, distGlobal.at(id), 1, MPI::INT);
        //form offsets
        offsets[0]  = 0;
        for ( int p = 1; p < int(_nPart.at(id)); p++ ){
           offsets[p]  = distGlobal.at(id)[p-1]  + offsets[p-1];
        } 

        //gathering partial partions for _row and _col
        MPI::COMM_WORLD.Allgatherv(_elemWithGhosts.at(id), _nelemsWithGhosts.at(id), MPI::INT, elemGlobal.at(id), 
                                distGlobal.at(id), offsets, MPI::INT);


       //shift  distGlobal to one right
        for ( int i = int(_nPart.at(id)); i > 0; i--)
           distGlobal.at(id)[i] = distGlobal.at(id)[i-1];

        distGlobal.at(id)[0] = 0;

       //summing distGlobal
        for ( int i = 1; i < _nPart.at(id)+1; i++ ){
             distGlobal.at(id)[i] += distGlobal.at(id)[i-1];
        }
        delete [] offsets;


      //forming CRConnectivity for cellPart
       int nghost = _totElemsAndGhosts.at(id) - _totElems.at(id);
      _cellSiteGlobal.push_back( StorageSitePtr(new StorageSite(_totElems.at(id), nghost )) );
      _partSite.push_back( StorageSitePtr(new StorageSite(_nPart.at(id)) )  );

       _cellParts.push_back( CRConnectivityPtr(new CRConnectivity( *_cellSiteGlobal.at(id), *_partSite.at(id)) ) );

      _cellParts.at(id)->initCount();

      for ( int indx = 0; indx < _totElemsAndGhosts.at(id); indx++)
          _cellParts.at(id)->addCount(indx,1);

      _cellParts.at(id)->finishCount();

      int index = 0;
      while ( index < _nPart.at(id) ){
         for ( int n = distGlobal.at(id)[index]; n < distGlobal.at(id)[index+1]; n++){

             _cellParts.at(id)->add(elemGlobal.at(id)[n],index);
         }
         index++;
      }

      _cellParts.at(id)->finishAdd();
      _partCells.push_back( _cellParts.at(id)->getTranspose() );
   }


    //deleting allocated arrays in this method
    vector< int* > ::iterator it_int;
    for ( it_int = elemGlobal.begin(); it_int != elemGlobal.end(); it_int++)
       delete [] *it_int;

    for ( it_int = distGlobal.begin(); it_int != distGlobal.end(); it_int++)
       delete [] *it_int;

    if ( _debugMode )
         DEBUG_CRConnectivity_cellParts();

}

//debug
void
MeshPartitioner::DEBUG_CRConnectivity_cellParts()
{
    //open file
    debug_file_open("CRConnectivity_cellParts");
    //_cellParts
    _debugFile << " _cellParts : " << endl;
    _debugFile << endl;
    _debugFile << " _cellParts->getRowDim() = " << _cellParts.at(0)->getRowDim() << endl;
    _debugFile << " _cellParts->getColDim() = " << _cellParts.at(0)->getColDim() << endl;
    _debugFile << endl;
    const Array<int>&   rowCellParts = _cellParts.at(0)->getRow();
    const Array<int>&   colCellParts = _cellParts.at(0)->getCol();
    for ( int n  = 0;n < _cellParts.at(0)->getRowDim(); n++){
        _debugFile << " row[" << n << "] = " << rowCellParts[n] << "    ";
        int nnodes = rowCellParts[n+1] - rowCellParts[n];
        for ( int node = 0; node < nnodes; node++){
             _debugFile  << colCellParts[ rowCellParts[n] + node ] << "    ";
        }
        _debugFile << endl;
    }
   //close file
   debug_file_close();

}

//construct CRConnectivity faceParts
void
MeshPartitioner::CRConnectivity_faceParts()
{
     for ( int id = 0; id < _nmesh; id++){
          _faceCellsGlobal.push_back( &_meshList.at(id)->getAllFaceCells() );
          _faceNodesGlobal.push_back( &_meshList.at(id)->getAllFaceNodes() );

          _faceParts.push_back( _faceCellsGlobal.at(id)->multiply( *_cellParts.at(id), false) );
          _partFaces.push_back( _faceParts.at(id)->getTranspose() );
          _partNodes.push_back( _partFaces.at(id)->multiply( *_faceNodesGlobal.at(id), false) );
    }

    if ( _debugMode )
       DEBUG_CRConnectivity_faceParts();
}

//debug
void
MeshPartitioner::DEBUG_CRConnectivity_faceParts()
{
   //open file
   debug_file_open("CRConnectivity_faceParts");
   //faceParts
   _debugFile << " _faceParts : " << endl;
   _debugFile << endl;
   _debugFile << " _faceParts->getRowDim() = " << _faceParts.at(0)->getRowDim() << endl;
   _debugFile << " _faceParts->getColDim() = " << _faceParts.at(0)->getColDim() << endl;
    const Array<int>&   rowFaceParts = _faceParts.at(0)->getRow();
    const Array<int>&   colFaceParts = _faceParts.at(0)->getCol();
    for ( int n = 0; n < _faceParts.at(0)->getRowDim();n++){
        _debugFile << " row[" << n <<"] = " ;
        int nnodes = rowFaceParts[n+1] - rowFaceParts[n];
        for ( int node = 0; node < nnodes; node++){
            _debugFile << colFaceParts[ rowFaceParts[n] + node ] << "    ";
        }
        _debugFile << endl;
    }

    _debugFile << endl;
   //close file
   debug_file_close();


}



//forming faceCells and faceNodes on each local mesh
void 
MeshPartitioner::faceCells_faceNodes()
{
    vector< ArrayIntPtr  >   indices;
 
    for ( int id = 0; id < _nmesh; id++){
        //form site 
        int face_count = _partFaces.at(id)->getCount( _procID );
        int node_count = _partNodes.at(id)->getCount( _procID );

        _faceSite.push_back( StorageSitePtr(new  StorageSite(face_count)) );
        _nodeSite.push_back( StorageSitePtr(new  StorageSite(node_count)) );

        const Array<int>&  row = _partFaces.at(id)->getRow();
        const Array<int>&  col = _partFaces.at(id)->getCol();
        //forming indices
        indices.push_back( ArrayIntPtr(new Array<int>( face_count )) );
        int n_start = row[_procID];
        int indx = 0;
        for ( int n = n_start; n < n_start + face_count; n++){
            (*indices.at(id))[indx] = col[n];
            indx++;
        }
        //getting subset from global _faceCellsGlobal and _faceNodesGlobal
        int cell_count  = _nelems.at(id);
        int ghost_count = _nelemsWithGhosts.at(id) - _nelems.at(id) + _interfaceMap.at(id).size();
       _cellSite.push_back( StorageSitePtr(new StorageSite( cell_count, ghost_count)) );

       _faceCells.push_back( _faceCellsGlobal.at(id)->getLocalizedSubsetOfFaceCells( *_faceSite.at(id), *_cellSite.at(id), *indices.at(id), *_cellParts.at(id), _procID )  );
       _faceNodes.push_back( _faceNodesGlobal.at(id)->getLocalizedSubset( *_faceSite.at(id), *_nodeSite.at(id), *indices.at(id) )  );
       _cellCells.push_back( (_faceCells.at(id)->getTranspose())->multiply(*_faceCells.at(id), true) );
       _cellNodes.push_back( (_faceCells.at(id)->getTranspose())->multiply(*_faceNodes.at(id), false) );

    }

    if ( _cleanup )
       cleanup_follow_faceCells_faceNodes();

    if ( _debugMode )
        DEBUG_faceCells_faceNodes();
}


void
MeshPartitioner::DEBUG_faceCells_faceNodes()
{
   //open file
   debug_file_open("faceCells_faceNodes");
   _debugFile << "faceCells_faceNodes : " << endl;
   _debugFile << endl;

   const Array<int>&   globalToLocalMap = _faceCells.at(0)->getGlobalToLocalMap();
   const Array<int>&   localToGlobalMap = _faceCells.at(0)->getLocalToGlobalMap();
   _debugFile << " globalToLocalMap.length() = " << globalToLocalMap.getLength() << endl;
   for ( int n = 0; n < globalToLocalMap.getLength(); n++)
       _debugFile << " globalToLocalMap[" << n << "] = " << globalToLocalMap[n] << endl;
   _debugFile << endl;
   _debugFile << " localToGlobalMap.length() = " << localToGlobalMap.getLength() << endl;
   for ( int n = 0; n < _nelems.at(0); n++)
       _debugFile << " localToGlobalMap[" << n << "] = " << localToGlobalMap[n] << endl;
   _debugFile << endl;

   //faceCells 
   _debugFile << " _faceCells :  " << endl;
   _debugFile << " _faceCells->getRowDim() = " << _faceCells.at(0)->getRowDim() << endl;
   _debugFile << " _faceCells->getColDim() = " << _faceCells.at(0)->getColDim() << endl;
   const Array<int>&   rowFaceCells = _faceCells.at(0)->getRow();
   const Array<int>&   colFaceCells = _faceCells.at(0)->getCol();
   for ( int face = 0; face < _faceCells.at(0)->getRowDim(); face++){
      _debugFile << " row[" << face <<"] = " << (*_partFaces.at(0))(_procID,face) << "    ";
      int ncells = _faceCells.at(0)->getCount(face);
      for ( int cell = 0; cell < ncells; cell++){
         _debugFile << colFaceCells[ rowFaceCells[face] + cell ] << "    ";
      }
      _debugFile << endl;
   }
   _debugFile << endl;
   //faceNodes
   _debugFile << " _faceNodes :  " << endl;
   _debugFile << " _faceNodes->getRowDim() = " << _faceNodes.at(0)->getRowDim() << endl;
   _debugFile << " _faceNodes->getColDim() = " << _faceNodes.at(0)->getColDim() << endl;
   const Array<int>&   rowFaceNodes = _faceNodes.at(0)->getRow();
   const Array<int>&   colFaceNodes = _faceNodes.at(0)->getCol();

   for ( int face = 0; face < _faceNodes.at(0)->getRowDim(); face++){
       _debugFile << " row[" << face <<"] = " << (*_partFaces.at(0))(_procID,face) << "    ";
       int nnodes = _faceNodes.at(0)->getCount(face);
       for ( int node = 0; node < nnodes; node++){
           _debugFile << colFaceNodes[ rowFaceNodes[face] + node ] << "    ";
       }
       _debugFile << endl;
   }
   _debugFile << endl;
   //faceNodes
   _debugFile << " _cellNodes(Local Numbering) :  " << endl;
   _debugFile << " _cellNodes->getRowDim() = " << _cellNodes.at(0)->getRowDim() << endl;
   _debugFile << " _cellNodes->getColDim() = " << _cellNodes.at(0)->getColDim() << endl;
   const Array<int>&   rowCellNodes = _cellNodes.at(0)->getRow();
   const Array<int>&   colCellNodes = _cellNodes.at(0)->getCol();

   for ( int cell = 0; cell < _cellNodes.at(0)->getRowDim(); cell++){
       _debugFile << " row[" << cell  << "]  = " ;
       int nnodes = _cellNodes.at(0)->getCount(cell);
       for ( int node = 0; node < nnodes; node++){
          _debugFile << colCellNodes[ rowCellNodes[cell] + node ] << "    ";
       }
       _debugFile << endl;
   }
   _debugFile << endl;
   //cellCells
   _debugFile << " _cellCells :  " << endl;
   _debugFile << " _cellCells->getRowDim() = " << _cellCells.at(0)->getRowDim() << endl;
   _debugFile << " _cellCells->getColDim() = " << _cellCells.at(0)->getColDim() << endl;
   const Array<int>&   rowCellCells = _cellCells.at(0)->getRow();
   const Array<int>&   colCellCells = _cellCells.at(0)->getCol();

   for ( int cell = 0; cell < _cellCells.at(0)->getRowDim(); cell++){
       _debugFile << " row[" << cell <<"] = "  << "    ";
       int nnodes = _cellCells.at(0)->getCount(cell);
       for ( int node = 0; node < nnodes; node++){
          _debugFile << colCellCells[ rowCellCells[cell] + node ] << "    ";
       }
       _debugFile << endl;
   }
   _debugFile << endl;
   //close file
   debug_file_close();

}


void
MeshPartitioner::cleanup_follow_faceCells_faceNodes()
{
    MPI::COMM_WORLD.Barrier();
   _faceCellsGlobal.clear();
   _faceNodesGlobal.clear();
    vector< int* > ::iterator it_int;
    for ( it_int  = _elem.begin(); it_int != _elem.end(); it_int++)
       delete [] *it_int;


}

//form interfaces
void
MeshPartitioner::interfaces()
{
    _interfaceMap.resize( _nmesh );
    for ( int id = 0; id < _nmesh; id++){
       int nface =   _partFaces.at(id)->getCount( _procID );
        for ( int face = 0; face < nface; face++ ){
             int face_globalID = (*_partFaces.at(id))(_procID,face);
            if (_faceParts.at(id)->getCount(face_globalID) == 2 ){  // ==2 means sharing interface
                int neighPart = (*_faceParts.at(id))(face_globalID,0) +           
                                (*_faceParts.at(id))(face_globalID,1) - _procID; 
                _interfaceSet.at(id).insert( neighPart );
                _interfaceMap.at(id).insert(  pair<int,int>(neighPart,face) );
            }
        }
    }

    if ( _debugMode )
       DEBUG_interfaces();
}

void
MeshPartitioner::DEBUG_interfaces()
{
    //open file
    debug_file_open("interfaces");
    //interfaceMap
    pair<multimap<int,int>::iterator,multimap<int,int>::iterator> ret;
    _debugFile << "_InterfaceMap : " << endl;
    _debugFile << endl;
    _debugFile << "_interfaceMap.size() = " <<  _interfaceMap.at(0).size() << endl;
    _debugFile << endl;
    multimap<int,int>::iterator it_multimap;
    for ( int part = 0; part < _nPart.at(0); part++ ){
         ret = _interfaceMap.at(0).equal_range(part);
         _debugFile  << " interface ID =  "  << part << "  =>  ";
         for (it_multimap=ret.first; it_multimap!=ret.second; ++it_multimap)
             _debugFile  <<  (*_partFaces.at(0))(_procID,  (*it_multimap).second) << "  ";
         _debugFile << endl;
    }
    _debugFile << endl;
    //close file
    debug_file_close();

}

//form coordinates 
void
MeshPartitioner::coordinates()
{

  for ( int id = 0; id < _nmesh; id++){
      const Mesh& mesh = *(_meshList.at(id));
        const Array<Mesh::VecD3>&   global_coord = mesh.getNodeCoordinates();
        int node_count = _partNodes.at(id)->getCount( _procID );
        _coord.push_back( ArrayVecD3Ptr(new Array<Mesh::VecD3>(node_count)) );
        const Array<int>&    rowPartNodes = _partNodes.at(id)->getRow();
        const Array<int>&    colPartNodes = _partNodes.at(id)->getCol();

        for ( int node = 0; node < node_count; node++)
            (*_coord.at(id))[node] = global_coord[ colPartNodes[rowPartNodes[_procID]+node] ];
    }
    
    if ( _debugMode ) 
        DEBUG_coordinates();

}

void
MeshPartitioner::DEBUG_coordinates()
{
     debug_file_open("coordinates");
    //node coordinates
    _debugFile << "coordinates : " << endl;
    _debugFile << endl;
    int node_count = _partNodes.at(0)->getCount( _procID );
    for ( int node = 0; node < node_count; node++){
        _debugFile << fixed;
        _debugFile << " node ID = " <<setw(10)<< node << setprecision(7) << ",  x = " << (*_coord.at(0))[node][0] << 
         setprecision(7) << ",  y = " << (*_coord.at(0))[node][1] << 
         setprecision(7) << ",  z = " << (*_coord.at(0))[node][2] << endl; 
    }
    _debugFile << endl;
     debug_file_close();
}

int
MeshPartitioner::count_interior_faces( int id )
{
   return _partFaces.at(id)->getCount(_procID) - (_nelemsWithGhosts.at(id) - _nelems.at(id)) 
          - _interfaceMap.at(id).size();

}


//this is expensive way, lets keep it
void
MeshPartitioner::non_interior_cells()
{
     for ( int id = 0; id < _nmesh; id++ ){
         int nface_local = _partFaces.at(id)->getCount( _procID );
         for ( int face = 0; face < nface_local; face++){
             int cell_0 = (*_faceCells.at(id))(face,0);
             int cell_1 = (*_faceCells.at(id))(face,1);
             if ( cell_0 >= _nelems.at(id) ) 
               _nonInteriorCells.at(id).insert(cell_0);
             if ( cell_1 >= _nelems.at(id) ) 
               _nonInteriorCells.at(id).insert(cell_1);

        }
    }

    if ( _debugMode )
        DEBUG_non_interior_cells();
}

void
MeshPartitioner::DEBUG_non_interior_cells()
{    
     //open
     debug_file_open("non_interior_cells");
     set<int>::const_iterator it_set;
     //non-interior cells (only for last mesh)
     _debugFile << "_nonInteriorCells : " << endl;
     _debugFile << endl;
     _debugFile << "total non-interior cells  = " << _nonInteriorCells.at(0).size() << endl;
     _debugFile << endl;
     for ( it_set = _nonInteriorCells.at(0).begin(); it_set != _nonInteriorCells.at(0).end(); it_set++ )
         _debugFile <<  "      " <<  *it_set  << endl;

     _debugFile << endl;
    //close
    debug_file_close();

}



void
MeshPartitioner::preserve_cell_order()
{

   const CRConnectivity& faceCells    = *_faceCells.at(0);
   const Array<int>& globalToLocalList = faceCells.getGlobalToLocalMap();
   const Array<int>& localToGlobalList = faceCells.getLocalToGlobalMap();
   set<int> globalCellList;
   //first copy _faceCells.GlobalTocell to some set which will be ordered from smallest integer to largest one
   for ( int i = 0; i < globalToLocalList.getLength(); i++){
       if ( globalToLocalList[i] != -1 ){
            globalCellList.insert( i );
       }    
   }
  //now use ordered set 
  int indx = 0;
  foreach(  const set<int>::value_type globalID, globalCellList ){
       int localID = globalToLocalList[globalID];
       _cellToPreservedOrderCell[localID] = indx++;
  }
  //use global to Local 
  for ( int i = 0; i < localToGlobalList.getLength(); i++ ){ 
      const int glblID = localToGlobalList[i];
      _globalToLocal[glblID] = i;
  }
  
  if ( _debugMode )
     DEBUG_preserve_cell_order();

}

void
MeshPartitioner::DEBUG_preserve_cell_order()
{
   //file open
   debug_file_open("preserve_cell_order");
   _debugFile << "_cellToPreservedOrderCell : " << endl;
   _debugFile << endl;
   foreach ( const IntMap::value_type& pos, _cellToPreservedOrderCell )
      _debugFile << pos.first << "       "  <<  pos.second << endl;

   _debugFile << endl;
   _debugFile << " _globalToLocal : " << endl;
   _debugFile << endl;

   foreach ( const IntMap::value_type& pos, _globalToLocal ){
      _debugFile << "glblID = " << pos.first << ",    localID  =  " <<  pos.second << endl;
   }

   //close
   debug_file_close();

}


//faceCells and faceNodes are order such that interior face come first and 
//then  boundary faces or interfaces follows after that
//interface and boundary cells which are always stored as second element 
//faceCellsOrdered(face,0) => interior cells, faceCellsOrdered(face,1)=>boundary or interface cells
void 
MeshPartitioner::order_faceCells_faceNodes()
{

     for ( int id = 0; id < _nmesh; id++ ){
        int tot_cells = _nelemsWithGhosts.at(id) + _interfaceMap.at(id).size();
        construct_mesh( id );

        _faceCellsOrdered.push_back( CRConnectivityPtr( new  CRConnectivity(_meshListLocal.at(id)->getFaces(), _meshListLocal.at(id)->getCells() ) ) );
        _faceNodesOrdered.push_back( CRConnectivityPtr( new  CRConnectivity(_meshListLocal.at(id)->getFaces(), _meshListLocal.at(id)->getNodes() ) ) );
        _cellToOrderedCell[id].assign(tot_cells, -1);
        //first preserve order cells (stick with global numbering)
         preserve_cell_order();
          //faceCells 
         _faceCellsOrdered.at(id)->initCount();
         _faceNodesOrdered.at(id)->initCount();

         int nface = _partFaces.at(id)->getCount(_procID);
         int count_node = _faceNodes.at(id)->getRow()[1] - _faceNodes.at(id)->getRow()[0];
         int count_cell = _faceCells.at(id)->getRow()[1] - _faceCells.at(id)->getRow()[0];
         for ( int face = 0; face < nface; face++){
            _faceCellsOrdered.at(id)->addCount(face,count_cell);  //two cells (always)
            _faceNodesOrdered.at(id)->addCount(face,count_node);  //two, three or four nodes
         }

         _faceCellsOrdered.at(id)->finishCount();
         _faceNodesOrdered.at(id)->finishCount();

         //start with interior faces
         int array_length = _faceCells.at(id)->getLocalToGlobalMap().getLength(); 
         assert( array_length == tot_cells );

         int face_track = 0;
         int nface_local = _partFaces.at(id)->getCount( _procID );
         for ( int face = 0; face < nface_local; face++){
             int cell_0 = (*_faceCells.at(id))(face,0);
             int cell_1 = (*_faceCells.at(id))(face,1);
             //find if this face is interior or not
             bool is_interior = _nonInteriorCells.at(id).count(cell_0) == 0 &&
                                _nonInteriorCells.at(id).count(cell_1) == 0; 

              if ( is_interior ) {
                   int cellID0 = _cellToPreservedOrderCell[cell_0];
                   int cellID1 = _cellToPreservedOrderCell[cell_1];
                    //map to cell number to preserved ordering mapping
                   _cellToOrderedCell[id][cell_0] = cellID0;
                   _cellToOrderedCell[id][cell_1] = cellID1;
                    //add operation to faceCells
                   _faceCellsOrdered.at(id)->add(face_track,cellID0);
                   _faceCellsOrdered.at(id)->add(face_track,cellID1);
                   //storing things as mappers
                   int globalID0 = _faceCells.at(id)->getLocalToGlobalMap()[cell_0];
                   int globalID1 = _faceCells.at(id)->getLocalToGlobalMap()[cell_1];
                   _globalToLocalMappers.at(id).insert( pair<int,int>(globalID0,cellID0 ) );
                   _globalToLocalMappers.at(id).insert( pair<int,int>(globalID1,cellID1 ) );
                   _localToGlobalMappers.at(id).insert( pair<int,int>(cellID0, globalID0) );
                   _localToGlobalMappers.at(id).insert( pair<int,int>(cellID1, globalID1) );
                   //faceNodesOrdered
                   for  ( int node = 0; node < count_node; node++ )
                      _faceNodesOrdered.at(id)->add( face_track, (*_faceNodes.at(id))( face, node ) );
                   face_track++;
              }
         }

         //check if any inner cells are not visited from above search (it might be inner cell 
         //surrounded by interface/boundary faces,so,the above loop fail to catch inner cells)
        foreach(const IntMap::value_type& mpos, _cellToPreservedOrderCell){
             int cellID     = mpos.first;
             int global_id = _faceCells.at(id)->getLocalToGlobalMap()[cellID];
             if ( _cellToOrderedCell[0][cellID] == -1 ){ //means not visited 
                 int orderedCellID = mpos.second;
                 _cellToOrderedCell[0][cellID] = orderedCellID;
                 _globalToLocalMappers.at(id).insert( pair<int,int>(global_id,orderedCellID)  );  
                 _localToGlobalMappers.at(id).insert( pair<int,int>(orderedCellID, global_id) );
             }
        }

       int cellID = _cellToPreservedOrderCell.size();
        //then boundary faces
       multimap<int,int>::const_iterator it_cell;
       pair<multimap<int,int>::const_iterator,multimap<int,int>::const_iterator> it;
       set<int> ::const_iterator it_set;
       int offset = face_track;
       //loop over boundaries
       for ( it_set = _boundarySet.at(id).begin(); it_set != _boundarySet.at(id).end(); it_set++){
          int bndryID = *it_set;
          it = _mapBounIDAndCell.at(id).equal_range(bndryID);
          //if it is not empty
          if ( _mapBounIDAndCell.at(id).count( bndryID ) > 0 )
             _bndryOffsets.at(id).insert( pair<int,int>(bndryID, offset) );
 
          for ( it_cell = it.first; it_cell != it.second; it_cell++ ){
               int elem_0 = _globalToLocal[it_cell->second];
               int elem_1 =  (*_cellCells.at(id))(elem_0, 0);
               assert( elem_0 != elem_1 );
               int inner_elem = _cellToOrderedCell[id][elem_1];
               int outer_elem = cellID;

               //update globalToLocal and localToGlobalMaps
               _globalToLocalMappers.at(id).insert( pair<int,int>(it_cell->second,cellID ) );
               _localToGlobalMappers.at(id).insert( pair<int,int>(cellID, it_cell->second) );
               _cellToOrderedCell[id][elem_0] = cellID;

               _faceCellsOrdered.at(id)->add(face_track, inner_elem);
               _faceCellsOrdered.at(id)->add(face_track, outer_elem);

               int count_node = _faceNodes.at(id)->getRow()[1] - _faceNodes.at(id)->getRow()[0];
               for  ( int node = 0; node < count_node; node++)
                    _faceNodesOrdered.at(id)->add( face_track, (*_cellNodes.at(id))(elem_0,node) );

              face_track++;
              offset++;
              cellID++;
          }
       }


       //then interface faces
       multimap<int,int>::const_iterator it_face;
       for ( it_set = _interfaceSet.at(id).begin(); it_set != _interfaceSet.at(id).end(); it_set++){
          int interfaceID = *it_set;
          it = _interfaceMap.at(id).equal_range( interfaceID );
          _interfaceOffsets.at(id).insert( pair<int,int>(interfaceID,offset) ) ;
          for ( it_face = it.first; it_face != it.second; it_face++ ){
              int face_id = it_face->second;
              int elem_0 =  (*_faceCells.at(id))(face_id,0);
              int elem_1 =  (*_faceCells.at(id))(face_id,1);
              int outer_elem_id = -1;

             if ( _nonInteriorCells.at(id).count( elem_1 ) > 0 ){ //if elem_1 is non-interior cell
                _faceCellsOrdered.at(id)->add(face_track,_cellToOrderedCell[id][elem_0]);
                 outer_elem_id = elem_1;
             } else { 
                _faceCellsOrdered.at(id)->add(face_track,_cellToOrderedCell[id][elem_1]);
                outer_elem_id = elem_0;
             }
            _faceCellsOrdered.at(id)->add(face_track,cellID);

             //update maps
             int global_id = _faceCells.at(id)->getLocalToGlobalMap()[outer_elem_id];
             assert( cellID >=0 && cellID < array_length );
             _globalToLocalMappers.at(id).insert( pair<int,int>(global_id, cellID) );
             _localToGlobalMappers.at(id).insert( pair<int,int>(cellID, global_id) );
             _cellToOrderedCell[id][outer_elem_id] = cellID;

             int count_node = _faceNodes.at(id)->getRow()[1] - _faceNodes.at(id)->getRow()[0];

             if ( outer_elem_id == elem_1 ) {
                 for  ( int  node = 0; node < count_node; node++)
                    _faceNodesOrdered.at(id)->add( face_track, (*_faceNodes.at(id))( face_id, node ) );
             } else {
                 for  ( int  node = count_node-1; node >= 0; node--)
                    _faceNodesOrdered.at(id)->add( face_track, (*_faceNodes.at(id))( face_id, node ) );
             }

              face_track++;
              offset++;
              cellID++;
          }

       }

        _faceCellsOrdered.at(id)->finishAdd(); 
        _faceNodesOrdered.at(id)->finishAdd();
        assert(  cellID == tot_cells );
     }


     if ( _debugMode )
         DEBUG_order_faceCells_faceNodes();

}

void
MeshPartitioner::DEBUG_order_faceCells_faceNodes()
{
    //openfile
    debug_file_open("order_faceCells_faceNodes");
    //faceCellsOrdered
    _debugFile << " _faceCellsOrdered :  " << endl;
    _debugFile << " _faceCellsOrdered->getRowDim() = " << _faceCellsOrdered.at(0)->getRowDim() << endl;
    _debugFile << " _faceCellsOrdered->getColDim() = " << _faceCellsOrdered.at(0)->getColDim() << endl;
    const Array<int>& rowFaceCellsOrdered = _faceCellsOrdered.at(0)->getRow();
    const Array<int>& colFaceCellsOrdered = _faceCellsOrdered.at(0)->getCol();
    for ( int face = 0; face < _faceCellsOrdered.at(0)->getRowDim(); face++){
        _debugFile << " row[" << face <<"] = " ;
        int ncells = _faceCellsOrdered.at(0)->getCount(face);
        for ( int cell = 0; cell < ncells; cell++){
           _debugFile << colFaceCellsOrdered[ rowFaceCellsOrdered[face] + cell ]  << "    ";
        }
        _debugFile << endl;
     }
     _debugFile << endl;

    //faceNodes
    _debugFile << " _faceNodesOrdered :  " << endl;
    _debugFile << " _faceNodesOrdered->getRowDim() = " << _faceNodesOrdered.at(0)->getRowDim() << endl;
    _debugFile << " _faceNodesOrdered->getColDim() = " << _faceNodesOrdered.at(0)->getColDim() << endl;
    const Array<int>& rowFaceNodesOrdered = _faceNodesOrdered.at(0)->getRow();
    const Array<int>& colFaceNodesOrdered = _faceNodesOrdered.at(0)->getCol();

    for ( int face = 0; face < _faceNodesOrdered.at(0)->getRowDim(); face++){
       _debugFile << " row[" << face<<"] = " ;
       int nnodes = _faceNodesOrdered.at(0)->getCount(face);
       for ( int node = 0; node < nnodes; node++){
           _debugFile << colFaceNodesOrdered[ rowFaceNodesOrdered[face] + node ]+1 << "    ";
       }
       _debugFile << endl;
    }
    _debugFile << endl;

   //close
   debug_file_close();
}

void 
MeshPartitioner::construct_mesh( int id )
{
        int dim = _meshList.at(id)->getDimension();
        _meshListLocal.push_back(  new Mesh( dim, _procID)   );

        StorageSite& faceSite = _meshListLocal.at(id)->getFaces();
        StorageSite& cellSite = _meshListLocal.at(id)->getCells();
        StorageSite& nodeSite = _meshListLocal.at(id)->getNodes();
        int nface_local = _partFaces.at(id)->getCount( _procID );
        int tot_cells = _nelemsWithGhosts.at(id) + _interfaceMap.at(id).size();
        int nGhostCell_local =  tot_cells - _nelems.at(id);
        int nnode_local =_partNodes.at(id)->getCount( _procID );

        //Storage sites
        faceSite.setCount( nface_local );
        cellSite.setCount( _nelems.at(id), nGhostCell_local );
        nodeSite.setCount( nnode_local );
}

//collect each mesh neightbourhood interface cells, ...
void 
MeshPartitioner::exchange_interface_meshes()
{

    vector<int>  offset;
    vector<int>  interfaceMeshIDs;
    int *recv_counts = NULL;
    int *displ       = NULL;
    for ( int id = 0; id < _nmesh; id++){
        recv_counts = new int[ _nPart.at(id) ];
        displ       = new int[ _nPart.at(id) ];

        int total_interface_mesh = int( _interfaceSet.at(id).size() );
        int total_faces = int( _interfaceMap.at(id).size() );

        MPI::COMM_WORLD.Allgather(&total_interface_mesh, 1, MPI::INT, _interfaceMeshCounts.at(id)->getData(), 1, MPI::INT);
        MPI::COMM_WORLD.Allgather(&total_faces, 1, MPI::INT, _procTotalInterfaces.at(id)->getData(), 1, MPI::INT);

        //now find offsets for ghostCells 
        int total_interface_local = _interfaceSet.at(id).size();
        int total_interface_global = -1;

        MPI::COMM_WORLD.Allreduce( &total_interface_local, &total_interface_global, 1, MPI::INT, MPI::SUM );
        MPI::COMM_WORLD.Allgather( &total_interface_local, 1, MPI::INT, recv_counts, 1, MPI::INT );
        MPI::COMM_WORLD.Allreduce( &total_faces, &_windowSize.at(id), 1, MPI::INT, MPI::MAX);

      //enough space for gathering
       _offsetInterfaceCells.push_back(  ArrayIntPtr( new Array<int>(total_interface_global) )  );
       _interfaceMeshIDs.push_back    (  ArrayIntPtr( new Array<int>(total_interface_global) )  ); 

       _ghostCellsGlobal.push_back    (  ArrayIntPtr( new Array<int>(total_faces ) )  );
       _ghostCellsLocal.push_back     (  ArrayIntPtr( new Array<int>(total_faces ) )  );

        //local offset and interfaceMeshID are stored in contigous memory
        int index = 0;
        set<int>::const_iterator it_set;
        for ( it_set = _interfaceSet.at(id).begin(); it_set != _interfaceSet.at(id).end(); it_set++ ){
            int neighMeshID = *it_set;
            interfaceMeshIDs.push_back( neighMeshID );
            //loop over interface
             int  nstart = _interfaceOffsets.at(id)[neighMeshID];
             offset.push_back( nstart );

             int  nend  = nstart + _interfaceMap.at(id).count( neighMeshID );
            for ( int n = nstart;  n < nend; n++){
                  int elem_local_id  = (*_faceCellsOrdered.at(id))(n,0);
                  int elem_global_id = _localToGlobalMappers.at(id)[ elem_local_id ];

                 (*_ghostCellsLocal.at(id))[index]  = elem_local_id;
                 (*_ghostCellsGlobal.at(id))[index] = elem_global_id;
                 index++;
            }

        }

        displ[0] = 0;
        for ( int i = 1; i < _nPart.at(id); i++)
            displ[i] = recv_counts[i-1] + displ[i-1]; 


         //now gather for _interface...
         MPI::COMM_WORLD.Allgatherv( &offset[0], total_interface_local, MPI::INT, 
                  _offsetInterfaceCells.at(id)->getData(), recv_counts, displ, MPI::INT); 

         MPI::COMM_WORLD.Allgatherv( &interfaceMeshIDs[0], total_interface_local, MPI::INT, 
                  _interfaceMeshIDs.at(id)->getData(), recv_counts, displ, MPI::INT); 

       offset.clear();
       interfaceMeshIDs.clear();
 
       delete [] recv_counts;
       delete [] displ;

    }

   if ( _debugMode )
      DEBUG_exchange_interface_meshes();

}

void 
MeshPartitioner::DEBUG_exchange_interface_meshes()
{
     debug_file_open("exchange_interface_meshes");
     //interfaceMesheCounts
     for ( int proc = 0; proc < _nPart.at(0); proc++)
         _debugFile << " total mesh surrounding = " << (*_interfaceMeshCounts.at(0))[proc] << endl;
     _debugFile << endl;

     //ofsest
     _debugFile << " offset for ghost Cells from adjacent meshes to read data from _ghostCellsGlobal : "  << endl;
     for ( int n = 0; n < _offsetInterfaceCells.at(0)->getLength(); n++ )
         _debugFile << "    n  =  " << n << " offsetInterfaceCells = " << (*_offsetInterfaceCells.at(0))[n] << endl;
     _debugFile << endl;
     //interfaceMeshIDs
     _debugFile << " neightboorhood cell IDs : "  << endl;
     for ( int n = 0; n < _interfaceMeshIDs.at(0)->getLength(); n++ )
        _debugFile << "    n  =  " << n << "  interfaced Mesh ID = " <<  (*_interfaceMeshIDs.at(0))[n] << endl;
     _debugFile << endl;

     //global Interface cells (interior ones, global numbering)
     _debugFile << "interface cells looking interior domain (global numbering)  : " << endl;
      for ( int n = 0; n < _ghostCellsGlobal.at(0)->getLength(); n++ )
         _debugFile << "    n  =  " << n << "  cell ID = " <<  (*_ghostCellsGlobal.at(0))[n] << endl;

     //global Interface cells (interior ones, global numbering)
     _debugFile << "interface cells looking interior domain (local numbering)  : " << endl;
     for ( int n = 0; n < _ghostCellsLocal.at(0)->getLength(); n++ )
         _debugFile << "    n  =  " << n << "  interfaced Mesh ID = " <<  (*_ghostCellsLocal.at(0))[n] << endl;
     debug_file_close();

}

void
MeshPartitioner::mappers()
{

    for ( int id = 0; id < _nmesh; id++){
        create_window( id );
        fence_window();

        StorageSite::ScatterMap & cellScatterMap = _meshListLocal.at(id)->getCells().getScatterMap();
        StorageSite::GatherMap  & cellGatherMap  = _meshListLocal.at(id)->getCells().getGatherMap();

       //getting data
        set<int>::const_iterator it_set;
        int interfaceIndx = 0;
        for ( it_set = _interfaceSet.at(id).begin(); it_set != _interfaceSet.at(id).end(); it_set++){

            int neighMeshID = *it_set;
            int size = int(_interfaceMap.at(id).count(neighMeshID) );
            _fromIndices.at(id).push_back( ArrayIntPtr( new Array<int>(size) ) );
              _toIndices.at(id).push_back( ArrayIntPtr( new Array<int>(size) ) );
           *_fromIndices.at(id).at(interfaceIndx)  = -1;
              *_toIndices.at(id).at(interfaceIndx) = -1;

             int window_displ = -1;
             window_displ = get_window_displ( id, neighMeshID );
            _winLocal.Get ( _fromIndices.at(id).at(interfaceIndx)->getData(), size, MPI::INT, neighMeshID, window_displ, size, MPI::INT );
            _winGlobal.Get( _toIndices.at(id).at(interfaceIndx)->getData()  , size, MPI::INT, neighMeshID, window_displ, size, MPI::INT );
             interfaceIndx++;

        }

       fence_window();
       free_window();


       interfaceIndx = 0;
       for ( it_set = _interfaceSet.at(id).begin(); it_set != _interfaceSet.at(id).end(); it_set++ ){

           int neighMeshID = *it_set;
           int size = int(_interfaceMap.at(id).count(neighMeshID) );
           map<int, int>  mapKeyCount;        //map between key and count of that key
           for ( int n = 0; n < size; n++){

               int  key     = (*_toIndices.at(id).at(interfaceIndx))[n];
//               int  count   = _globalToLocalMappers.at(id).count( key ); 

               if ( mapKeyCount.count( key ) > 0 ) { //it has elements
                  mapKeyCount[key] = mapKeyCount[key] + 1; //increase one
               } else {  //if it is empty  
                  mapKeyCount.insert(pair<int,int>(key,0));
               }

               multimap<int,int>::const_iterator it;
               it = _globalToLocalMappers.at(id).lower_bound( key );
               for ( int n_iter = 0; n_iter < mapKeyCount[key]; n_iter++)
                   it++;

               int elem_id =  it->second;
               (*_toIndices.at(id).at(interfaceIndx))[n] =  elem_id;


           }
          //from indices seems useless for now but we need to find scatterCells = cellCells(toIndices) and
          //use fromindices as storage Array
          for ( int i = 0; i < _fromIndices.at(id).at(interfaceIndx)->getLength(); i++){
               int elem_id = (*_toIndices.at(id).at(interfaceIndx))[i];
                (*_fromIndices.at(id).at(interfaceIndx))[i] = _meshListLocal.at(id)->getCellCells()(elem_id,0);
          }
          Mesh::PartIDMeshIDPair pairID = make_pair<int,int>(neighMeshID,0);
          cellScatterMap[ _meshListLocal.at(id)->getGhostCellSiteScatter( pairID ) ] = _fromIndices.at(id).at(interfaceIndx);
          cellGatherMap [ _meshListLocal.at(id)->getGhostCellSiteGather ( pairID ) ] = _toIndices.at(id).at(interfaceIndx);
 
          interfaceIndx++;

        }

    }

    if ( _cleanup )
       cleanup_follow_mappers();

   if ( _debugMode )
      DEBUG_mesh();

}


void 
MeshPartitioner::cleanup_follow_mappers()
{
    MPI::COMM_WORLD.Barrier();
   _localToGlobalMappers.clear();
   _globalToLocalMappers.clear();
}

int
MeshPartitioner::get_window_displ( int id, int neigh_mesh_id )
{
      int loc = 0;
      int window_displ = 0;
      for ( int i = 0; i < neigh_mesh_id; i++)
          loc += (*_interfaceMeshCounts.at(id))[i];
         
      while ( (*_interfaceMeshIDs.at(id))[loc] != _procID){
          window_displ += (*_offsetInterfaceCells.at(id))[loc+1] - (*_offsetInterfaceCells.at(id))[loc];
         loc++;
      }


    return window_displ;
}

void
MeshPartitioner::create_window( int id )
{
           int int_size = MPI::INT.Get_size();
           MPI::Aint lb, sizeofAint;
           MPI::INT.Get_extent(lb,sizeofAint);

           int window_size = _windowSize.at(id);  //already MPI::MAX  has taken  maximum size
           _winLocal  = MPI::Win::Create(_ghostCellsLocal.at(id)->getData(), window_size*sizeofAint, int_size,
                                         MPI_INFO_NULL, MPI::COMM_WORLD);
           _winGlobal = MPI::Win::Create(_ghostCellsGlobal.at(id)->getData(), window_size*sizeofAint, int_size,
                                         MPI_INFO_NULL, MPI::COMM_WORLD);

}

void
MeshPartitioner::free_window( )
{
     _winLocal.Free();
     _winGlobal.Free();
}

void 
MeshPartitioner::fence_window()
{

//     _winLocal.Fence ( 0 );
//     _winGlobal.Fence( 0 );

      _winLocal.Fence(MPI::MODE_NOPUT);
      _winGlobal.Fence(MPI::MODE_NOPUT);

}

//dump all mesh information 
void
MeshPartitioner::DEBUG_mesh()
{
    mesh_file();
    mesh_tecplot();


}

void 
MeshPartitioner::mesh_file()
{
     stringstream ss;
     ss << "mesh_proc" << _procID << "_info.dat";
     ofstream  mesh_file( (ss.str()).c_str() );
     for ( int id = 0; id < _nmesh; id++ ){

          const StorageSite::ScatterMap& cellScatterMap  = _meshListLocal.at(id)->getCells().getScatterMap();
          const StorageSite::GatherMap& cellGatherMap   = _meshListLocal.at(id)->getCells().getGatherMap();
          const Mesh::GhostCellSiteMap& ghostCellSiteScatterMap = _meshListLocal.at(id)->getGhostCellSiteScatterMap();
          const Mesh::GhostCellSiteMap& ghostCellSiteGatherMap  = _meshListLocal.at(id)->getGhostCellSiteGatherMap();

           Mesh::GhostCellSiteMap::const_iterator it_ghostScatter;
            //loop over interfaces
           for ( it_ghostScatter = ghostCellSiteScatterMap.begin(); it_ghostScatter != ghostCellSiteScatterMap.end(); it_ghostScatter++ ){
               const Mesh::PartIDMeshIDPair pairID = it_ghostScatter->first;
               int neighID = pairID.first;

               const StorageSite& siteScatter =  *( ghostCellSiteScatterMap.find( pairID )->second );
               const StorageSite& siteGather  =  *( ghostCellSiteGatherMap.find ( pairID )->second );


               const Array<int>&  scatterArray =  *(cellScatterMap.find( &siteScatter )->second);
               const Array<int>&  gatherArray  =  *(cellGatherMap.find ( &siteGather )->second);
               for ( int i = 0; i < siteScatter.getCount(); i++){
                     mesh_file <<   "  neightMeshID = " <<  neighID  << "        "
                               << gatherArray[i]  + 1  << "    ===>    " 
                               << scatterArray[i] + 1  << endl;
               }
          }

      }

      mesh_file.close();

}


//need modification for quad, hexa, tetra
void
MeshPartitioner::mesh_tecplot()
{
     stringstream ss;
     ss << "mesh_proc" << _procID << ".dat";
     ofstream  mesh_file( (ss.str()).c_str() );

      const Mesh& mesh = *(_meshListLocal.at(0));
      const CRConnectivity&  cellNodes = mesh.getCellNodes();
      const Array<Mesh::VecD3>&  coord = mesh.getNodeCoordinates();
      int tot_elems = cellNodes.getRowDim();
      int tot_nodes =  coord.getLength();

     mesh_file << "title = \" tecplot file for process Mesh \" "  << endl;
     mesh_file << "variables = \"x\",  \"y\", \"z\", \"cell_type\" " << endl;
#if 0
     mesh_file << "variables = \"x\",  \"y\", \"z\", \"cell_type\", \"color\"  " << endl;
#endif

     stringstream zone_info;

     if ( _eType.at(0) == TRI ) 
        zone_info <<  " DATAPACKING = BLOCK,  VARLOCATION = ([4]=CELLCENTERED), ZONETYPE=FETRIANGLE ";

     if ( _eType.at(0) == QUAD ) 
        zone_info <<  " DATAPACKING = BLOCK,  VARLOCATION = ([4]=CELLCENTERED), ZONETYPE=FEQUADRILATERAL ";

     if ( _eType.at(0) == HEXA ) 
        zone_info <<  " DATAPACKING = BLOCK,  VARLOCATION = ([4]=CELLCENTERED), ZONETYPE=FEBRICK ";

     if ( _eType.at(0) == TETRA ) 
        zone_info <<  " DATAPACKING = BLOCK,  VARLOCATION = ([4]=CELLCENTERED), ZONETYPE=FETETRAHEDRON ";


     mesh_file << "zone N = " << tot_nodes << " E = " << tot_elems << zone_info.str()  << endl;

     //x 
     for ( int n = 0; n < tot_nodes; n++){
         mesh_file << scientific  << coord[n][0] << "     " ;
         if ( n % 5 == 0 ) mesh_file << endl;
     }
     mesh_file << endl;

     //y
     for ( int n= 0; n < tot_nodes; n++){
         mesh_file << scientific << coord[n][1] << "     ";
         if ( n % 5 == 0 ) mesh_file << endl;
     }
     mesh_file << endl;

     //z
     for ( int n = 0; n < tot_nodes; n++){
          mesh_file << scientific << coord[n][2] << "     ";
          if ( n % 5 == 0) mesh_file << endl;
     }

     mesh_file << endl;
     mesh_file << endl;
     //cell type
      int cell_type = -1;
      for ( int n = 0; n < tot_elems;  n++){
          int elem_id = _cellToOrderedCell[0][n];
          cell_type = 1;
          if ( _nonInteriorCells.at(0).count(elem_id) == 0 ){
             cell_type = 0;
          } else {
             cell_type = 1;
          }

          mesh_file << cell_type  << "      ";
          if (  n % 10 == 0 ) mesh_file << endl;

       }
    mesh_file << endl;
#if  0
      mesh_file << endl;
      //mesh color is only 
      const Array<int>& color = mesh.getCellColors();
      for ( int n = 0; n < tot_elems; n++ ){
          mesh_file <<  color[n] <<  "      ";
          if ( n % 10 == 0 ) mesh_file << endl;
      }
#endif

      
      mesh_file << endl;
     //connectivity
    for (int n = 0; n < tot_elems; n++){
         int nnodes =  cellNodes.getCount(n);
         if (  n < _nelems.at(0) ){
            for ( int node = 0; node < nnodes; node++)
                mesh_file << cellNodes(n,node)+1 << "      ";
         } else {

              if ( _eType.at(0) == TRI )
                  mesh_file << cellNodes(n,0)+1 << "      " << cellNodes(n,1)+1 <<
                   "       " << cellNodes(n,0)+1 << "      ";

              if ( _eType.at(0) == QUAD )
                  mesh_file << cellNodes(n,0)+1 << "      "  << cellNodes(n,0)+1 <<
                   "       " << cellNodes(n,1)+1 << "      " << cellNodes(n,1)+1 << "      ";

              if ( _eType.at(0) == HEXA )
                  mesh_file << cellNodes(n,0)+1 << "      "  << cellNodes(n,1)+1 << "      "
                            << cellNodes(n,2)+1 << "      "  << cellNodes(n,3)+1 << "      "
                            << cellNodes(n,0)+1 << "      "  << cellNodes(n,1)+1 << "      "
                            << cellNodes(n,2)+1 << "      "  << cellNodes(n,3)+1 << "      ";

             if ( _eType.at(0) == TETRA ) 
                  mesh_file << cellNodes(n,0)+1 << "      "  << cellNodes(n,1)+1 <<
                   "       " << cellNodes(n,2)+1 << "      " << cellNodes(n,0)+1 << "      ";
                  

         }
        mesh_file << endl;
     }


    mesh_file.close();

}

void
MeshPartitioner::mesh_xdmf_header()
{
  int nprocs = MPI::COMM_WORLD.Get_size();
  ofstream  mesh_file("mesh.xmf");
  mesh_file << "<?xml version='1.0' ?>" << endl;
  mesh_file << "<!DOCTYPE Xdmf SYSTEM 'Xdmf.dtd' []>" << endl;
  mesh_file << "<Xdmf xmlns:xi='http://www.w3.org/2001/XInclude' Version='2.0'>" << endl;
  mesh_file << "  <Domain>" << endl;
  for (int i=0; i < nprocs; i++)
    mesh_file << "    <xi:include href='mesh_proc" << i << ".xmf' />" << endl;
  mesh_file << "  </Domain>" << endl;
  mesh_file << "</Xdmf>" << endl;
  mesh_file.close();
}


// Dump the mesh in xdmf format.
// We use ASCII for convenience with the values encoded in the XML, 
// rather that a seperate hdf5 file. 
void
MeshPartitioner::mesh_xdmfplot()
{
  if (_procID == 0)
    mesh_xdmf_header();
      
  stringstream ss;
  ss << "mesh_proc" << _procID << ".xmf";
  ofstream  mesh_file( (ss.str()).c_str() );
  
  const Mesh& mesh = *(_meshListLocal.at(0));
  const CRConnectivity&  cellNodes = mesh.getCellNodes();
  const Array<Mesh::VecD3>&  coord = mesh.getNodeCoordinates();
  int tot_elems = _nelems.at(0);
  int tot_nodes =  coord.getLength();
  int epn;

  mesh_file << "<Grid Name='Mesh-" << _procID << "' GridType='Uniform'>" << endl << "  ";
  
  switch (_eType.at(0)) {
  case TRI:
    mesh_file << "<Topology TopologyType='Triangle'";
    epn = 3;
    break;
  case QUAD:
    mesh_file << "<Topology TopologyType='Quadrilateral'";
    epn = 4;
    break;
  case HEXA:
    mesh_file << "<Topology TopologyType='Hexahedron'";
    epn = 8;
    break;
  case TETRA:
    mesh_file << "<Topology TopologyType='Tetrahedron'";
    epn = 4;
    break;
  default:
    cout << "Unknown mesh type " << _eType.at(0) << endl;
    return;
  }
  mesh_file << " Dimensions='" << tot_elems << "'>" << endl;
  mesh_file << "    <DataItem Dimensions='" << tot_elems << " " << epn << "'>" << endl;

  //connectivity (Topology)
  for (int n = 0; n < tot_elems; n++) {
    mesh_file << "      ";
    for (int node = 0; node < cellNodes.getCount(n); node++)
	mesh_file << cellNodes(n,node) << " ";
    mesh_file << endl;
  }
  mesh_file << "    </DataItem>" << endl;
  mesh_file << "  </Topology>" << endl;

  // Geometry
  mesh_file << "  <Geometry Type='XYZ'>" << endl;
  mesh_file << "    <DataItem Dimensions='" << tot_nodes << " 3' NumberType='Float'>" << endl;
  for (int n = 0; n < tot_nodes; n++) {
    mesh_file << "      ";
    mesh_file << coord[n][0] << " " ;
    mesh_file << coord[n][1] << " " ;
    mesh_file << coord[n][2] << endl;
  }
  mesh_file << "    </DataItem>" << endl;     
  mesh_file << "  </Geometry>" << endl;
  mesh_file << "</Grid>" << endl;
  mesh_file.close();
}



void 
MeshPartitioner::CRConnectivityPrint( const CRConnectivity& conn, int procID, const string& name )
{

   if ( MPI::COMM_WORLD.Get_rank() == procID ){
      cout <<  name << " :" << endl;
      const Array<int>& row = conn.getRow();
      const Array<int>& col = conn.getCol();
      for ( int i = 0; i < row.getLength()-1; i++ ){
         cout << " i = " << i << ",    ";
         for ( int j = row[i]; j < row[i+1]; j++ )
            cout << col[j] << "  ";
         cout << endl;
      }
   }

}


void 
MeshPartitioner::CRConnectivityPrintFile( const CRConnectivity& conn, const string& name )
{
      _debugFile <<  name << " :" << endl;
      _debugFile << endl;
      const Array<int>& row = conn.getRow();
      const Array<int>& col = conn.getCol();
      for ( int i = 0; i < row.getLength()-1; i++ ){
         _debugFile << " i = " << i << ",    ";
         for ( int j = row[i]; j < row[i+1]; j++ )
            _debugFile << col[j] << "  ";
         _debugFile << endl;
      }
      _debugFile << endl;
}

void
MeshPartitioner::debug_file_open( const string& fname_ )
{  
    stringstream ss(stringstream::in | stringstream::out);
    ss <<  _procID;

     string  fname = "MeshPartitioner_PROC" + ss.str() + "_" +  fname_ + ".dat";
    _debugFile.open( fname.c_str() );
    ss.str("");
}

void
MeshPartitioner::debug_file_close()
{
    _debugFile.close();
}
