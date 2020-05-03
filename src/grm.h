#ifndef SNPLIB_SRC_GRM_H_
#define SNPLIB_SRC_GRM_H_

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

#include "transpose_geno.h"
#include "unpack_geno.h"

namespace snplib {
void CalcGRMMatrix(const uint8_t *geno, const double *af, size_t num_samples,
                   size_t num_snps, double *matrix, size_t num_threads);
void CalcGCTADiagonal(const uint8_t *geno, const double *af, size_t num_samples,
                      size_t num_snps, double *diagonal, size_t num_threads);
}  // namespace snplib

#endif  // SNPLIB_SRC_GRM_H_
