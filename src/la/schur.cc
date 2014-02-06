/*!
 *
 * Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
 *
 * Please make a following refer to Flow123d on your project site if you use the program for any purpose,
 * especially for academic research:
 * Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License version 3 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
 *
 *
 * $Id$
 * $Revision$
 * $LastChangedBy$
 * $LastChangedDate$
 *
 * @file
 * @ingroup la
 * @brief  Assembly explicit Schur complement for the given linear system.
 * Provides method for resolution of the full original vector of unknowns.
 *
 * Aim: Explicit schur should be faster then implicit, i.e.
 *
 * @todo
 * - vyresit navaznost na lin sys - solve a export seq vektoru, redukce ... ?
 * - inv_a - predava se pri konstrukci, ale neumoznuje jeji reuse - aktualizaci assemblace
 *   resp. nutno si na ni drzet ukazatel venku ... reseni ?
 * - ? remove old_4_new - just for LSView
 * - automatic preallocation
 * - eliminated block given by IS
 * - in place Schur
 * - ? nemodifikovat puvodni system, leda skrze jeho metody
 */

#include <petscvec.h>
#include <algorithm>
#include <limits>
#include <petscmat.h>
#include <armadillo>

#include "la/distribution.hh"
#include "la/local_to_global_map.hh"
//#include "solve.h"
#include "system/system.hh"
#include "la/linsys.hh"
#include "la/linsys_BDDC.hh"
#include "la/schur.hh"

/**
 *  Create Schur complement system.
 *  @param[in] orig  : original system
 *  @param[in] inv_a : inversion of the A block
 *  @param[in] ia    : index set of the A block,
 *   default continuous given by inv_a:
 *   proc    1      2      3
 *
 *   Orig:   ****** ****** ****
 *   IA  :   ***    **     ***
 *
  *
 */

SchurComplement::SchurComplement(IS ia, Distribution *ds)
: LinSys_PETSC(ds, MPI_COMM_WORLD), IsA(ia), state(created), Orig(NULL)
{
        xprintf(Msg, "Constructor SchurComplement\n");

        // check index set
        ASSERT(IsA != NULL, "Index set IsA is not defined.\n" );

        // initialize variables
        Compl   = NULL;
        IA_sub  = NULL;
        B       = NULL;
        Bt      = NULL;
        B_sub   = NULL;
        Bt_sub  = NULL;
        xA      = NULL;
        xA_sub  = NULL;
        IAB     = NULL;
        IAB_sub = NULL;
        IsB     = NULL;
        fullIsA = NULL;
        fullIsB = NULL;
        RHS1    = NULL;
        RHS2    = NULL;
        Sol1    = NULL;
        Sol2    = NULL;
        sub_vec_block2 = NULL;
}


SchurComplement :: SchurComplement(LinSys *orig, IS ia, Distribution *ds)
: LinSys_PETSC(ds, MPI_COMM_WORLD), IsA(ia), state(created), Orig(orig)
{
        xprintf(Msg, "Constructor SchurComplement\n");

        PetscInt m, n;
        PetscErrorCode ierr;
        PetscScalar *rhs_array, *sol_array;
        int orig_first;

        // check type of LinSys, dimensions of matrix, index set
        ierr = MatGetSize(Orig->get_matrix(), &m, &n);
        //ASSERT(typeid(*Orig) == typeid(LinSys_PETSC), "Assumed PETSC type of Orig object.\n");
        ASSERT(m == n, "Assumed square matrix.\n" );
        ASSERT(IsA != NULL, "Index set IsA is not defined.\n" );

        // initialize variables
        Compl   = NULL;
        IA_sub  = NULL;
        B       = NULL;
        Bt      = NULL;
        B_sub   = NULL;
        Bt_sub  = NULL;
        xA      = NULL;
        xA_sub  = NULL;
        IAB     = NULL;
        IAB_sub = NULL;
        sub_vec_block2 = NULL;

        F_ENTRY;

	    // get distribution of original matrix
	    MatGetOwnershipRange(Orig->get_matrix(),&orig_first,PETSC_NULL);
	    MatGetLocalSize(Orig->get_matrix(),&orig_lsize,PETSC_NULL);

	    // create A block index set
	    ISGetLocalSize(IsA, &loc_size_A);
	    ISAllGather(IsA,&fullIsA);
	    //ISView(IsA, PETSC_VIEWER_STDOUT_WORLD);

	    // create B block index set
	    locSizeB = orig_lsize-loc_size_A;
	    ISCreateStride(PETSC_COMM_WORLD,locSizeB,orig_first+loc_size_A,1,&IsB);
	    ISAllGather(IsB,&fullIsB);
	    //ISView(IsB, PETSC_VIEWER_STDOUT_WORLD);

	    // create complement system
	    // TODO: introduce LS as true object, clarify its internal states
	    // create RHS sub vecs RHS1, RHS2
	    VecGetArray(Orig->get_rhs(),&rhs_array);
	    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,loc_size_A,PETSC_DETERMINE,rhs_array,&(RHS1));

	    // create Solution sub vecs Sol1, Compl->solution
	    VecGetArray(Orig->get_solution(),&sol_array);
	    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,loc_size_A,PETSC_DETERMINE,sol_array,&(Sol1));

	    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,locSizeB,PETSC_DETERMINE,rhs_array+loc_size_A,&(RHS2));
	    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,locSizeB,PETSC_DETERMINE,sol_array+loc_size_A,&(Sol2));

	    VecRestoreArray(Orig->get_rhs(),&rhs_array);
	    VecRestoreArray(Orig->get_solution(),&sol_array);

	    VecGetArray( Sol2, &sol_array );
	    ds_ = new Distribution(locSizeB, PETSC_COMM_WORLD);
	    this->set_solution(sol_array);
	    this->set_from_input( Orig->in_rec_ );
	    VecRestoreArray( Sol2, &sol_array );

        create_inversion_matrix();
}


/**
 *  COMPUTE A SCHUR COMPLEMENT OF A PETSC MATRIX
 *
 *  given symmetric original matrix Orig has form
 *  A  B     x_1    RHS_1
 *  B' C  *  x_2  = RHS_2
 *  where the first block is given by index set IsA, and the second block by IsB
 *  user has to provide inverse IA of the A-block
 *  we suppose that original matrix have non-zero pattern for the schur complement
 *
 *  we return: Shur - schur complement, ShurRHS - RHS of the complemented system:
 *  (B' * IA * B - C) * x_2 = (B' * IA * RHS_1 - RHS_2)
 *  IAB - a matrix to compute eliminated part of the solution:
 *  x_1 = IA * RHS_1 - IAB * x_2
 *
 *  Actually as B' is stored separetly, the routine can be used also for nonsymetric original
 * system
 *
 */

void SchurComplement::form_schur()
{
    PetscErrorCode ierr = 0;
    MatReuse mat_reuse;        // reuse structures after first computation of schur

    mat_reuse=MAT_REUSE_MATRIX;
    if (state==created) mat_reuse=MAT_INITIAL_MATRIX; // indicate first construction

    //DBGMSG("Compute Schur complement of\n");
    //MatView(Orig->get_matrix(),PETSC_VIEWER_STDOUT_WORLD);
    //DBGMSG("inverse IA:\n");
    //MatView(Schur->IA,PETSC_VIEWER_STDOUT_WORLD);
    // compose Schur complement
    // Petsc need some fill estimate for results of multiplication in form nnz(A*B)/(nnz(A)+nnz(B))
    // for the first Schur compl: IA*B is bounded by ( d*(d+1) )/( d*d+2*d ) <= 5/6 for d<=4
    //                            B'*IA*B bounded by ( (d+1)*(d+1) )/ ( d*(d+1) + d ) ~ 1
    // for the second Schur :      IA*B have fill ratio ~ 1.
    //                            B'*IA*B  ...         ( N/2 *(2*N-1) )/( 2 + 2*N ) <= 1.4
    // nevertheless Petsc does not allows fill ratio below 1. so we use 1.1 for the first
    // and 1.5 for the second multiplication

    // compute IAB=IA*B, locSizeB removed
    ierr+=MatGetSubMatrix(Orig->get_matrix(), IsA, IsB, mat_reuse, &B);
    //DBGMSG(" B:\n");
    //MatView(Schur->B,PETSC_VIEWER_STDOUT_WORLD);
    ierr+=MatMatMult(IA, B, mat_reuse, 1.0 ,&(IAB)); // 6/7 - fill estimate
    //DBGMSG(" IAB:\n");
    //MatView(Schur->IAB,PETSC_VIEWER_STDOUT_WORLD);
    // compute xA=Bt* IAB = Bt * IA * B, locSizeA removed
    ierr+=MatGetSubMatrix(Orig->get_matrix(), IsB, IsA, mat_reuse, &(Bt));
    ierr+=MatMatMult(Bt, IAB, mat_reuse, 1.9 ,&(xA)); // 1.1 - fill estimate (PETSC report values over 1.8)
    //DBGMSG("xA:\n");
    //MatView(Schur->xA,PETSC_VIEWER_STDOUT_WORLD);

    // get C block, locSizeB removed
    ierr+=MatGetSubMatrix( Orig->get_matrix(), IsB, IsB, mat_reuse, const_cast<Mat *>( &(this->get_matrix()) ) );
    // compute complement = (-1)cA+xA = Bt*IA*B - C
    ierr+=MatScale(this->get_matrix(),-1.0);
    //DBGMSG("C block:\n");

    //MatView(Schur->Compl->A,PETSC_VIEWER_STDOUT_WORLD);
    ierr+=MatAXPY(this->get_matrix(), 1, xA, SUBSET_NONZERO_PATTERN);
    //DBGMSG("C block:\n");
    //MatView(Schur->Compl->A,PETSC_VIEWER_STDOUT_WORLD);
    //
    //TODO: MatAXPY - umoznuje nasobit -1, t.j. bylo by lepe vytvorit konvencni Schuruv doplnek zde,
    // a ve funkci schur1 (a ne v schur2) uzit metodu "scale" z tohoto objektu - kvuli negativni definitnosti
    // usetri se tim jeden MatScale

    ASSERT( ierr == 0, "PETSC Error during calculation of Schur complement.\n");

    /*
    PetscViewerASCIIOpen(PETSC_COMM_WORLD,"matAinv.output",&myViewer);
    PetscViewerSetFormat(myViewer,PETSC_VIEWER_ASCII_MATLAB);
    MatView( IA, myViewer );
    PetscViewerDestroy(myViewer);

    PetscViewerASCIIOpen(PETSC_COMM_WORLD,"matSchur.output",&myViewer);
    PetscViewerSetFormat(myViewer,PETSC_VIEWER_ASCII_MATLAB);
    MatView( Compl->get_matrix( ), myViewer );
    PetscViewerDestroy(myViewer);
*/

    form_rhs();

    state=formed;

}

void SchurComplement::form_rhs()
{
    MatMultTranspose(IAB,RHS1,this->get_rhs());
    VecAXPY(this->get_rhs(),-1,RHS2);

    state=formed;
}

/**
 *  @brief Scale formed complement system. Mainly to make it positive definite.
 */

void SchurComplement ::scale(double scalar)
{
    ASSERT( state == formed, "Object in wrong state!\n");
    MatScale(this->get_matrix(), scalar);
    VecScale(this->get_rhs(), scalar);
}

/**
 * COMPUTE ELIMINATED PART OF THE ORIG. SYS. & RESTORE RHS and SOLUTION VECTORS
 *  x_1 = IA * RHS_1 - IAB * x_2
 */

void SchurComplement::resolve()
{
    F_ENTRY;

    MatMult(IAB,this->get_solution(),Sol1);

    VecScale(Sol1,-1);

    MatMultAdd(IA,RHS1,Sol1,Sol1);

}

void SchurComplement::set_complement(LinSys_PETSC *ls)
{
    PetscScalar *sol_array;

    Compl = ls;
    VecGetArray( Sol2, &sol_array );
    Compl->set_solution( sol_array );
    Compl->set_from_input( in_rec_ );
    VecRestoreArray( Sol2, &sol_array );
}

Distribution *SchurComplement::make_complement_distribution()
{
    PetscInt m, n;
    PetscErrorCode ierr;
    PetscScalar *rhs_array, *sol_array;
    int orig_first;

    // check dimensions of matrix
    ierr = MatGetSize(matrix_, &m, &n);
    ASSERT(m == n, "Assumed square matrix.\n" );

    F_ENTRY;

    // get distribution of original matrix
    MatGetOwnershipRange(matrix_,&orig_first,PETSC_NULL);
    MatGetLocalSize(matrix_,&orig_lsize,PETSC_NULL);

    // create A block index set
    ISGetLocalSize(IsA, &loc_size_A);
    ISAllGather(IsA,&fullIsA);
    //ISView(IsA, PETSC_VIEWER_STDOUT_WORLD);

    // create B block index set
    locSizeB = orig_lsize-loc_size_A;
    ISCreateStride(PETSC_COMM_WORLD,locSizeB,orig_first+loc_size_A,1,&IsB);
    ISAllGather(IsB,&fullIsB);
    //ISView(IsB, PETSC_VIEWER_STDOUT_WORLD);

    // create complement system
    // TODO: introduce LS as true object, clarify its internal states
    // create RHS sub vecs RHS1, RHS2
    VecGetArray(rhs_, &rhs_array);
    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,loc_size_A,PETSC_DETERMINE,rhs_array,&(RHS1));

    // create Solution sub vecs Sol1, Compl->solution
    VecGetArray(solution_, &sol_array);
    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,loc_size_A,PETSC_DETERMINE,sol_array,&(Sol1));

    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,locSizeB,PETSC_DETERMINE,rhs_array+loc_size_A,&(RHS2));
    VecCreateMPIWithArray(PETSC_COMM_WORLD,1,locSizeB,PETSC_DETERMINE,sol_array+loc_size_A,&(Sol2));

    VecRestoreArray(rhs_, &rhs_array);
    VecRestoreArray(solution_, &sol_array);

    ds_ = new Distribution(locSizeB, PETSC_COMM_WORLD);
	return ds_;
}

void SchurComplement::create_inversion_matrix()
{
    PetscErrorCode ierr;
    PetscInt ncols, pos_start, pos_start_IA;

    MatGetSubMatrix(Orig->get_matrix(), IsA, IsA, MAT_INITIAL_MATRIX, &IA);
    //MatView(IA,PETSC_VIEWER_STDOUT_WORLD);
    MatGetOwnershipRange(Orig->get_matrix(),&pos_start,PETSC_NULL);
    MatGetOwnershipRange(IA,&pos_start_IA,PETSC_NULL);

    std::vector<PetscInt> submat_rows;
    const PetscInt *cols;
    const PetscScalar *vals;

    std::vector<unsigned int> processed_rows(loc_size_A,0);

    unsigned int mat_block=1;   //actual processed block of matrix
    for(unsigned int loc_row=0; loc_row < processed_rows.size(); loc_row++) {
        if (processed_rows[loc_row] != 0) continue;

        PetscInt min=std::numeric_limits<int>::max(), max=-1, size_submat;
        unsigned int b_vals = 0; // count of values stored in B-block of Orig system
        submat_rows.clear();
        ierr = MatGetRow(Orig->get_matrix(), loc_row + pos_start, &ncols, &cols, PETSC_NULL);
        for (PetscInt i=0; i<ncols; i++) {
            if (cols[i] < pos_start || cols[i] >= pos_start+loc_size_A) {
                b_vals++;
            } else {
                if (cols[i] < min) {
                    min=cols[i];
                }
                if (cols[i] > max) {
                    max=cols[i];
                }
            }
        }
        size_submat = max - min + 1;
        ASSERT(ncols-b_vals == size_submat, "Submatrix cannot contains empty values.\n");

        ierr = MatRestoreRow(Orig->get_matrix(), loc_row + pos_start, &ncols, &cols, PETSC_NULL);
        arma::mat submat2(size_submat, size_submat);
        submat2.zeros();
        for (PetscInt i=0; i<size_submat; i++) {
            processed_rows[ loc_row + i ] = mat_block;
            submat_rows.push_back( i + loc_row + pos_start_IA );
            ierr = MatGetRow(Orig->get_matrix(), i + loc_row + pos_start, &ncols, &cols, &vals);
            for (PetscInt j=0; j<ncols; j++) {
                if (cols[j] >= pos_start && cols[j] < pos_start+loc_size_A) {
                    submat2( i, cols[j] - loc_row - pos_start ) = vals[j];
                }
            }
            ierr = MatRestoreRow(Orig->get_matrix(), i + loc_row + pos_start, &ncols, &cols, &vals);
		}
        // test output
//            xprintf(Msg, "__ Get submat: rank %d, MIN-MAX %d %d, size %d\n", rank, min, max, size_submat);
//            for (int i=0; i<size_submat; i++) {
//                for (int j=0; j<size_submat; j++) {
//                    xprintf(Msg, "%2.0f ", submat2(i,j));
//                }
//                xprintf(Msg, "\n");
//            }
//            xprintf(Msg, "\n");
        // get inversion matrix
        arma::mat invmat = submat2.i();
        // stored to inversion IA matrix
        const PetscInt* rows = &submat_rows[0];
        MatSetValues(IA, submat_rows.size(), rows, submat_rows.size(), rows, invmat.memptr(), INSERT_VALUES);

        mat_block++;
    }

    MatAssemblyBegin(IA, MAT_FINAL_ASSEMBLY);
    MatAssemblyEnd(IA, MAT_FINAL_ASSEMBLY);
}


/**
 * SCHUR COMPLEMENT destructor
 */
SchurComplement :: ~SchurComplement() {

    F_ENTRY;

    if ( B  != NULL )             MatDestroy(&B);
    if ( Bt != NULL )             MatDestroy(&Bt);
    if ( xA != NULL )             MatDestroy(&xA);
    if ( IAB != NULL )            MatDestroy(&IAB);
    if ( IsA != NULL )            ISDestroy(&IsA);
    if ( IsB != NULL )            ISDestroy(&IsB);
    if ( fullIsA != NULL )        ISDestroy(&fullIsA);
    if ( fullIsB != NULL )        ISDestroy(&fullIsB);
    if ( RHS1 != NULL )           VecDestroy(&RHS1);
    if ( RHS2 != NULL )           VecDestroy(&RHS2);
    if ( Sol1 != NULL )           VecDestroy(&Sol1);
    if ( Sol2 != NULL )           VecDestroy(&Sol2);
    if ( B_sub != NULL )          MatDestroy(&B_sub);
    if ( Bt_sub != NULL )         MatDestroy(&Bt_sub);
    if ( xA_sub != NULL )         MatDestroy(&xA_sub);
    if ( IAB_sub != NULL )        MatDestroy(&IAB_sub);
    if ( sub_vec_block2 != NULL ) VecDestroy(&sub_vec_block2);

    if (Compl != NULL)            delete Compl;

}
