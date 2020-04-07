#include "statistics.h"

namespace snplib {

void CalcAlleleFrequencies(uint8_t *geno, size_t num_samples, size_t num_snps,
                           double *af) {}
void CalcMissing(uint8_t *geno, size_t num_samples, size_t num_snps,
                 double *cr) {}
void CalcAdjustedAF(uint8_t *geno, size_t num_samples, size_t num_snps,
                    double *covariates, size_t num_covariates, double *af,
                    size_t num_threads) {}
void CalcAdjustedMAF(uint8_t *geno, size_t num_samples, size_t num_snps,
                     double *covariates, size_t num_covariates, double *min_maf,
                     size_t num_threads) {}
void CalcSNPGFT(const double *covariates, const double *af, uint8_t *geno,
                size_t num_samples, size_t num_covariates, size_t num_snps,
                double *gft, size_t num_threads) {}
}  // namespace snplib