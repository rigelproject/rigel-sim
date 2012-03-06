#ifndef __SMATRIX_H__
#define __SMATRIX_H__

#include "cg.h"
#include "rigel.h"
#include <stdlib.h>
#include <stdio.h>

//////////////////////////////////////////////////
// Simple Yale Sparse Matrix Format ADT
//////////////////////////////////////////////////
int
SMatGetRowSize(const SparseMatrix *A, int row) {
	int size = A->IA[row+1] - A->IA[row];

	return size;
}
void
ElemTypeListEntrySet(ElemTypeListEntry *elem, int row, int col, ElemType value) {
	elem->row = row;
	elem->col = col;
	elem->value = value;
}


void 
SMatInit(SparseMatrix *A, int rows, int cols, const ElemTypeListEntry *list) {
	int i = 0, j = 0;
	int *col_offsets = (int*)malloc(sizeof(int)*(MATRIX_DIM+1+NONZERO_ELEMS));

	int *temp = (int *)malloc(sizeof(int)*(cols+rows));

	A->NumRows = rows;
	A->NumCols = cols;
	A->IA = __IA;
	A->IJ = __IJ;
	A->MatData = __MatData;

	// Calculate the offset into IJ for IA
	for (i = 0; i < rows+1; i++) col_offsets[i] = 0;
	for (i = 0; list[i].row != -1; i++) {
		col_offsets[list[i].row]++;
	#ifdef DEBUG
	printf("col_offsets[%d]: %d\n", list[i].row, col_offsets[list[i].row]);
	#endif
	}
	A->NonZeroes = i;
	
	for (i = 1, A->IA[0] = 0; i < rows+1; i++) { 
		A->IA[i] = col_offsets[i-1];
		col_offsets[i] += col_offsets[i-1]; 
	}
	
	// Use col_offsets to hold the current offset into a particular row
	for (i = 0; i < rows; i++) col_offsets[i] = 0;
	for (i = 0; i < A->NonZeroes; i++) {
		int curr_row = list[i].row;
		int idx = A->IA[curr_row]+col_offsets[curr_row];
		col_offsets[curr_row]++;

		// Save the column
		A->IJ[idx] = list[i].col;
		// Extract element data from list
		A->MatData[idx] = list[i].value;
	#ifdef DEBUG
	printf("Inserting idx: %d val: %e col: %d\n", idx, list[i].value, list[i].col);
	#endif
	}
	#ifdef DEBUG
	for (i = 0; i < rows+1; i++) { printf("IA[%d]: %d\n", i, A->IA[i]); }
	for (i = 0; i < NONZERO_ELEMS; i++) { printf("IJ[%d]: %d\n", i, A->IJ[i]); }
	for (i = 0; i < NONZERO_ELEMS; i++) {	printf("M[%d]: %e\n", i, A->MatData[i]); }
	#endif
	free(col_offsets); col_offsets = NULL;
	free(temp); temp = NULL;
}
	
ElemType 
SMatGetElem(const SparseMatrix *A, int row, int idx, int *col) {
	int ia = A->IA[row];

	*col = A->IJ[ia+idx];

	return A->MatData[ia+idx];
}

//////////////////////////////////////////////////
// Vector Operations
//////////////////////////////////////////////////
void 
Task_SMVMRows(const SparseMatrix *A, const ElemType *vec_in, 
	ElemType *vec_out, TQ_TaskDesc *tdesc) 
{
	int row, idx, col;
	ElemType data;
	const int end = tdesc->end;
	const int start_row = tdesc->begin;

	for (row = start_row; row < end; row++) {
		int row_size = SMatGetRowSize(A, row);
		vec_out[row] = 0.0f;
		idx = 0;
		int ia;
		int col0, col1, col2, col3, col4, col5, col6, col7;
		ElemType data0, data1, data2, data3, data4, data5, data6, data7;



    // TODO: Note for the future:  We could probably set up a smart prefetch
    // pipeline so that we are prefetching the column indices in one stage, the
    // matrix data and vector data in the next, and then do the computation in
    // the next. The problem remains, however, that vec_in is not fully used
    // most of the time so only some of the entries per cache line are used from
    // vec_in.  However, by the end of the SMVM, it is possible a lot of them
    // will get touched.  The problem then becomes having too much data in that
    // vector to fit it into the cache (it can be >512 KiB even for our measly
    // datasets).  Oh well.
		while (idx < row_size - 8) {
      // XXX PREFETCH XXX
#ifdef SW_PREFETCH
      RigelPrefetchLine(&A->MatData[ia+idx+PREFETCH_DISTANCE_WORDS]);
#endif

			ia = A->IA[row];
			col0 = A->IJ[ia+idx+0];
			col1 = A->IJ[ia+idx+1];
			col2 = A->IJ[ia+idx+2];
			col3 = A->IJ[ia+idx+3];
			col4 = A->IJ[ia+idx+4];
			col5 = A->IJ[ia+idx+5];
			col6 = A->IJ[ia+idx+6];
			col7 = A->IJ[ia+idx+7];
			data0 = A->MatData[ia+idx+0];
			data1 = A->MatData[ia+idx+1];
			data2 = A->MatData[ia+idx+2];
			data3 = A->MatData[ia+idx+3];
			data4 = A->MatData[ia+idx+4];
			data5 = A->MatData[ia+idx+5];
			data6 = A->MatData[ia+idx+6];
			data7 = A->MatData[ia+idx+7];
			vec_out[row] += vec_in[col0] * data0;
			vec_out[row] += vec_in[col1] * data1;
			vec_out[row] += vec_in[col2] * data2;
			vec_out[row] += vec_in[col3] * data3;
			vec_out[row] += vec_in[col4] * data4;
			vec_out[row] += vec_in[col5] * data5;
			vec_out[row] += vec_in[col6] * data6;
			vec_out[row] += vec_in[col7] * data7;
			idx+=8;
		}
		for (; idx < row_size; idx++) {
			data = SMatGetElem(A, row, idx, &col);
			vec_out[row] += vec_in[col] * data;
		}
	}
}

void
Task_VecDot(const ElemType *a, const ElemType *b, int dim, ElemType *ret) {
	int i = 0;
	ElemType out = 0.0f;

	while (i < dim - 8) {
		ElemType a0 = a[i+0];
		ElemType a1 = a[i+1];
		ElemType a2 = a[i+2];
		ElemType a3 = a[i+3];
		ElemType a4 = a[i+4];
		ElemType a5 = a[i+5];
		ElemType a6 = a[i+6];
		ElemType a7 = a[i+7];
		out += a0 * b[i+0];
		out += a1 * b[i+1];
		out += a2 * b[i+2];
		out += a3 * b[i+3];
		out += a4 * b[i+4];
		out += a5 * b[i+5];
		out += a6 * b[i+6];
		out += a7 * b[i+7];
		i += 8;
	}
	for (; i < dim; i++) {
		out += a[i] * b[i];
	}

	*ret = out;

	return;
}

void 
VecScale(const ElemType scale, const ElemType *vecn, ElemType *vec_out, int dim) {
	int i;

	for (i = 0; i < dim; i++) {
		vec_out[i] = vecn[i] * scale;
	}
}

void 
Task_VecVecAdd(const ElemType *a, const ElemType *b, ElemType *out, int dim) {
	int i = 0;

	while (i < dim - 8) {
		ElemType a0 = a[i+0];
		ElemType a1 = a[i+1];
		ElemType a2 = a[i+2];
		ElemType a3 = a[i+3];
		ElemType a4 = a[i+4];
		ElemType a5 = a[i+5];
		ElemType a6 = a[i+6];
		ElemType a7 = a[i+7];
		out[i+0] = a0 + b[i+0];
		out[i+1] = a1 + b[i+1];
		out[i+2] = a2 + b[i+2];
		out[i+3] = a3 + b[i+3];
		out[i+4] = a4 + b[i+4];
		out[i+5] = a5 + b[i+5];
		out[i+6] = a6 + b[i+6];
		out[i+7] = a7 + b[i+7];
		i += 8;
	}
	for (; i < dim; i++) {
		out[i] = a[i] + b[i];
	}
}

void
Task_VecScaleAdd(const ElemType scale, const ElemType *a, const ElemType *b, ElemType *out, int dim)
{
	int i = 0;
	ElemType foo = 0.0f;

	while (i < dim - 8) {
		ElemType a0 = a[i+0] * scale;
		ElemType a1 = a[i+1] * scale;
		ElemType a2 = a[i+2] * scale;
		ElemType a3 = a[i+3] * scale;
		ElemType a4 = a[i+4] * scale;
		ElemType a5 = a[i+5] * scale;
		ElemType a6 = a[i+6] * scale;
		ElemType a7 = a[i+7] * scale;
		out[i+0] = a0 + b[i+0];
		out[i+1] = a1 + b[i+1];
		out[i+2] = a2 + b[i+2];
		out[i+3] = a3 + b[i+3];
		out[i+4] = a4 + b[i+4];
		out[i+5] = a5 + b[i+5];
		out[i+6] = a6 + b[i+6];
		out[i+7] = a7 + b[i+7];
		i += 8;
	}
	for (; i < dim; i++) {
		foo = scale*a[i];
		out[i] = foo + b[i];
	}
}

void
VecCopy(const ElemType *from, ElemType *to, int dim) {
	int i;
	for (i = 0; i < dim; i++) {
		to[i] = from[i];
	}
}

#endif
