#include "gwas.h"

namespace {
std::atomic_size_t index;
const std::array<double, 4> geno_table{0.0, 0.0, 1.0, 2.0};
const std::array<double, 4> mask_table{1.0, 0.0, 1.0, 1.0};
}  // namespace

namespace snplib {
void CalcLinearRegressionThread(const uint8_t *geno, size_t num_samples,
                                size_t num_snps, const double *covariates,
                                size_t num_covariates, const double *trait,
                                double *betas, double *stats) {
  auto num_full_bytes = num_samples / 4;
  auto num_samples_left = num_samples % 4;
  auto num_bytes = num_full_bytes + num_samples_left > 0 ? 1 : 0;
  auto *mask_d = new double[num_samples];
  auto *y = new double[num_samples];
  auto *cov = new double[num_samples * (num_covariates + 1)];
  auto *geno_d = cov + num_samples * num_covariates;
  auto *R = new double[(num_covariates + 1) * (num_covariates + 1)];
  auto *beta = new double[num_covariates + 1];
  auto m = static_cast<int32_t>(num_samples);
  auto n = static_cast<int32_t>(num_covariates + 1);
  auto local_index = index++;
  auto *tau = new double[num_covariates];
  double w1;
  LAPACKE_dgeqrf_work(LAPACK_COL_MAJOR, m, n, nullptr, m, nullptr, &w1, -1);
  double w2;
  LAPACKE_dorgqr_work(LAPACK_COL_MAJOR, m, n, n, nullptr, m, nullptr, &w2, -1);
  auto lwork = static_cast<int32_t>(w1 > w2 ? w1 : w2);
  auto *work = new double[lwork];
  while (local_index < num_snps) {
    std::copy(covariates, covariates + num_samples * num_covariates, cov);
    UnpackGeno(geno + local_index * num_bytes, num_samples, geno_table,
               mask_table, geno_d, mask_d);
    for (size_t i = 0; i < num_samples; ++i) {
      y[i] = trait[i] * mask_d[i];
    }
    for (size_t i = 0; i < num_covariates + 1; ++i) {
      auto *tmp_c = cov + i * num_samples;
      for (size_t j = 0; j < num_samples; ++j) {
        tmp_c[j] *= mask_d[j];
      }
    }
    LAPACKE_dgeqrf_work(LAPACK_COL_MAJOR, m, n, cov, m, tau, work, lwork);
    for (size_t i = 0; i < num_covariates + 1; ++i) {
      std::copy(cov + i * num_samples,
                cov + i * num_samples + num_covariates + 1,
                R + i * (num_covariates + 1));
    }
    LAPACKE_dorgqr_work(LAPACK_COL_MAJOR, m, n, n, cov, m, tau, work, lwork);
    cblas_dgemv(CblasColMajor, CblasTrans, m, n, 1.0, cov, m, y, 1, 0.0, beta,
                1);
    double var = 0.0;
    for (size_t i = 0; i < num_samples; ++i) {
      var += y[i] * y[i];
    }
    for (size_t i = 0; i < num_covariates + 1; ++i) {
      var -= beta[i] * beta[i];
    }
    var /= (std::accumulate(mask_d, mask_d + num_samples, 0.0) -
            num_covariates - 1);
    cblas_dtrsv(CblasColMajor, CblasUpper, CblasNoTrans, CblasNonUnit, n, R, n,
                beta, 1);
    betas[local_index] = beta[num_covariates];
    stats[local_index] =
        betas[local_index] /
        (std::sqrt(var) / R[(num_covariates + 1) * (num_covariates + 1) - 1]);
    local_index = index++;
  }
  delete[] mask_d;
  delete[] y;
  delete[] cov;
  delete[] R;
  delete[] beta;
  delete[] tau;
  delete[] work;
}
void CalcLinearRegressionGWAS(const uint8_t *geno, size_t num_samples,
                              size_t num_snps, const double *covariates,
                              size_t num_covariates, const double *trait,
                              double *betas, double *stats,
                              size_t num_threads) {
  std::vector<std::thread> workers;
  set_num_threads(1);
  index = 0;
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back(CalcLinearRegressionThread, geno, num_samples,
                         num_snps, covariates, num_covariates, trait, betas,
                         stats);
  }
  for (auto &&iter : workers) {
    iter.join();
  }
}
void CalcLogisticThread(const uint8_t *geno, size_t num_samples,
                        size_t num_snps, const double *covariates,
                        size_t num_covariates, const double *trait,
                        double *betas, double *stats) {
  auto num_full_bytes = num_samples / 4;
  auto num_samples_left = num_samples % 4;
  auto num_bytes = num_full_bytes + num_samples_left > 0 ? 1 : 0;
  auto *cov = new double[num_samples * (num_covariates + 1)];
  auto *geno_d = cov + num_samples * num_covariates;
  auto *mask_d = new double[num_samples];
  std::copy(covariates, covariates + num_samples * num_covariates, cov);
  LogisticRegress<1> worker_full(num_samples, num_covariates + 1);
  LogisticRegress<1> worker_reduce(num_samples, num_covariates);
  auto local_index = index++;
  while (local_index < num_snps) {
    UnpackGeno(geno + local_index * num_bytes, num_samples, geno_table,
               mask_table, geno_d, mask_d);
    worker_full.Estimate(cov, trait, mask_d);
    worker_reduce.Estimate(covariates, trait, mask_d);
    auto *beta = worker_full.GetBeta();
    betas[local_index] = beta[num_covariates];
    stats[local_index] =
        worker_full.CalcLikelihood(cov, trait, mask_d) -
        worker_reduce.CalcLikelihood(covariates, trait, mask_d);
    local_index = index++;
  }
  delete[] cov;
  delete[] mask_d;
}
void CalcLogisticGWAS(const uint8_t *geno, size_t num_samples, size_t num_snps,
                      const double *covariates, size_t num_covariates,
                      const double *trait, double *betas, double *stats,
                      size_t num_threads) {
  std::vector<std::thread> workers;
  set_num_threads(1);
  index = 0;
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back(CalcLogisticThread, geno, num_samples, num_snps,
                         covariates, num_covariates, trait, betas, stats);
  }
  for (auto &&iter : workers) {
    iter.join();
  }
}
void CalcCCAThread(const uint8_t *geno, size_t num_samples, size_t num_snps,
                   const double *trait, size_t num_dims, double *betas,
                   double *stats) {
  auto num_full_bytes = num_samples / 4;
  auto num_samples_left = num_samples % 4;
  auto num_bytes = num_full_bytes + num_samples_left > 0 ? 1 : 0;
  auto *Y = new double[num_samples * num_dims];
  auto *geno_d = new double[num_samples];
  auto *mask_d = new double[num_samples];
  auto *Sigma = new double[num_dims * num_dims];
  auto *d = new double[num_dims];
  auto *Rho = new double[num_dims];
  auto m = static_cast<int32_t>(num_samples);
  auto n = static_cast<int32_t>(num_dims);
  int32_t lwork;
  int32_t liwork;
  double w;
  LAPACKE_dsyevd_work(LAPACK_COL_MAJOR, 'V', 'U', n, nullptr, n, nullptr, &w,
                      -1, &liwork, -1);
  lwork = static_cast<int32_t>(w);
  auto *work = new double[lwork];
  auto *iwork = new int32_t[liwork];
  auto local_index = index++;
  while (local_index < num_snps) {
    UnpackGeno(geno + local_index * num_bytes, num_samples, geno_table,
               mask_table, geno_d, mask_d);
    auto *tmp_b = betas + local_index * num_dims;
    auto alpha = std::accumulate(mask_d, mask_d + num_samples, 0.0);
    for (size_t i = 0; i < num_dims; ++i) {
      auto *tmp_t = trait + i * num_samples;
      auto *tmp_y = Y + i * num_samples;

      for (size_t j = 0; j < num_samples; ++j) {
        tmp_y[j] = tmp_t[j] * mask_d[j];
      }
      auto mu = std::accumulate(tmp_y, tmp_y + num_samples, 0.0) / alpha;
      for (size_t j = 0; j < num_samples; ++j) {
        tmp_y[j] -= mu;
      }
    }
    auto mu = std::accumulate(geno_d, geno_d + num_samples, 0.0) / alpha;
    for (size_t i = 0; i < num_samples; ++i) {
      geno_d[i] -= mu;
    }
    auto a = 1.0 / (alpha - 1.0);
    double var_g = 0.0;
    for (size_t i = 0; i < num_samples; ++i) {
      var_g += geno_d[i] * geno_d[i];
    }
    var_g *= a;
    cblas_dsyrk(CblasColMajor, CblasLower, CblasTrans, n, m, a, Y, m, 0.0,
                Sigma, n);
    cblas_dgemv(CblasColMajor, CblasTrans, m, n, a, Y, m, geno_d, 1, 0.0, Rho,
                1);
    LAPACKE_dsyevd_work(LAPACK_COL_MAJOR, 'V', 'L', n, Sigma, n, d, work, lwork,
                        iwork, liwork);
    cblas_dgemv(CblasColMajor, CblasTrans, n, n, 1.0, Sigma, n, Rho, 1, 0.0,
                tmp_b, 1);
    for (size_t i = 0; i < num_dims; ++i) {
      tmp_b[i] /= d[i];
    }
    cblas_dgemv(CblasColMajor, CblasNoTrans, n, n, 1.0, Sigma, n, tmp_b, 1, 0.0,
                geno_d, 1);
    double rho = 0.0;
    for (size_t i = 0; i < num_dims; ++i) {
      rho += geno_d[i] * Rho[i];
    }
    stats[local_index] = -alpha * std::log(1 - rho);
    double norm = 0.0;
    for (size_t i = 0; i < num_dims; ++i) {
      norm += geno_d[i] * geno_d[i];
    }
    norm = std::sqrt(norm);
    for (size_t i = 0; i < num_dims; ++i) {
      tmp_b[i] /= norm;
    }
    local_index = index++;
  }
  delete[] Y;
  delete[] geno_d;
  delete[] mask_d;
  delete[] work;
  delete[] iwork;
}
void CalcCCAGWAS(const uint8_t *geno, size_t num_samples, size_t num_snps,
                 const double *trait, size_t num_dims, double *betas,
                 double *stats, size_t num_threads) {
  std::vector<std::thread> workers;
  set_num_threads(1);
  index = 0;
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back(CalcCCAThread, geno, num_samples, num_snps, trait,
                         num_dims, betas, stats);
  }
  for (auto &&iter : workers) {
    iter.join();
  }
}
void CalcUniLMMThread(const uint8_t *geno, size_t num_samples, size_t num_snps,
                      const double *lambda, const double *covariates,
                      size_t num_covariates, const double *trait, double *betas,
                      double *fstats, double *dfs) {
  auto num_full_bytes = num_samples / 4;
  auto num_samples_left = num_samples % 4;
  auto num_bytes = num_full_bytes + num_samples_left > 0 ? 1 : 0;
  auto *cov = new double[(num_covariates + 1) * num_samples];
  std::copy(covariates, covariates + num_samples * num_covariates, cov);
  UniLMM worker(lambda, cov, num_samples, num_covariates + 1);
  LineSearch<UniLMM> searcher(worker);
  auto *geno_d = cov + num_samples * num_covariates;
  auto *mask_d = new double[num_samples];
  auto *y = new double[num_samples];
  auto local_index = index++;
  while (local_index < num_snps) {
    UnpackGeno(geno + local_index * num_bytes, num_samples, geno_table,
               mask_table, geno_d, mask_d);
    for (size_t i = 0; i < num_samples; ++i) {
      y[i] = trait[i] * mask_d[i];
    }
    for (size_t i = 0; i < num_covariates + 1; ++i) {
      auto *tmp_c = cov + i * num_samples;
      for (size_t j = 0; j < num_samples; ++j) {
        tmp_c[j] *= mask_d[j];
      }
    }
    worker.CalcInitialGuess(y);
    double a;
    double f_old = worker.CalcLikelihood();
    for (size_t i = 0; i < 15; ++i) {
      worker.CalcEMStep();
      f_old = worker.CalcLikelihood();
    }
    worker.UpdateGradients();
    double f_new = f_old;
    for (size_t l = 0; l < 200; ++l) {
      f_old = f_new;
      worker.BackupVars();
      worker.UpdateHessian();
      worker.CalcAIStep();
      a = searcher.Search(f_old);
      f_new = searcher.GetFNew();
      if ((f_old - f_new) < 1e-8 * (1.0 + std::fabs(f_old))) {
        break;
      }
    }
    auto result = worker.CalcFTest();
    betas[local_index] = result[0];
    fstats[local_index] = result[1];
    dfs[local_index] = result[2];
    local_index = index++;
  }
  delete[] cov;
  delete[] mask_d;
  delete[] y;
}
void CalcUniLMMGWAS(const uint8_t *geno, size_t num_samples, size_t num_snps,
                    const double *lambda, const double *covariates,
                    size_t num_covariates, const double *trait, double *betas,
                    double *fstats, double *dfs, size_t num_threads) {
  std::vector<std::thread> workers;
  set_num_threads(1);
  index = 0;
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back(CalcUniLMMThread, geno, num_samples, num_snps, lambda,
                         covariates, num_covariates, trait, betas, fstats, dfs);
  }
  for (auto &&iter : workers) {
    iter.join();
  }
}
}  // namespace snplib