import math
import re
import numpy as np
import numpy.linalg as npl
from scipy import stats
import _SNPLIB as lib


chr_dict = {
    'X': 23,
    'Y': 24,
    'XY': 25,
    'MT': 26,
}


def convert_chr(chr_c):
    if chr_c.isdigit():
        return int(chr_c)
    else:
        return chr_dict[chr_c.upper()]


def CalcIBSConnection(src_geno, dest_geno, num_threads):
    return lib.CalcIBSConnection(src_geno.GENO, dest_geno.GENO, src_geno.nSamples, dest_geno.nSamples, num_threads)


class SNPLIB:
    def __init__(self, nThreads=1):
        self.nThreads = nThreads

    def importPLINKDATA(self, bfile):
        filename = bfile + '.bim'
        self.CHR = []
        self.RSID = []
        self.POS = []
        self.ALT = []
        self.REF = []
        with open(filename, 'r') as file:
            for line in file.readlines():
                strs = re.split(' |\t', line.strip())
                self.CHR.append(convert_chr(strs[0]))
                self.RSID.append(strs[1])
                self.POS.append(int(strs[3]))
                self.ALT.append(strs[4])
                self.REF.append(strs[5])
        self.nSNPs = len(self.CHR)
        filename = bfile + '.fam'
        self.FID = []
        self.IID = []
        self.PID = []
        self.MID = []
        self.Sex = []
        with open(filename, 'r') as file:
            for line in file.readlines():
                strs = re.split(' |\t', line.strip())
                self.FID.append(strs[0])
                self.IID.append(strs[1])
                self.PID.append(strs[2])
                self.MID.append(strs[3])
                self.Sex.append(int(strs[4]))
        self.nSamples = len(self.FID)
        filename = bfile + '.bed'
        num_bytes = math.ceil(self.nSamples / 4.0)
        GENO = np.fromfile(filename, dtype=np.uint8, count=-1)
        GENO = GENO[3:]
        GENO = np.reshape(GENO, (num_bytes, - 1))
        self.GENO = GENO.astype(dtype=np.uint8, order='F')

    # Statistics
    def CalcAlleleFrequencies(self):
        return lib.CalcAlleleFrequencies(self.GENO, self.nSamples)

    def CalcMissing(self):
        return lib.CalcMissing(self.GENO, self.nSamples)

    def CalcAdjustedAF(self, covariates):
        return lib.CalcAdjustedAF(self.GENO, covariates, self.nThreads)

    def CalcAdjustedMAF(self, covariates):
        return lib.CalcAdjustedMAF(self.GENO, covariates, self.nThreads)

    # Relationships
    def CalcAdjustedGRM(self, covariates):
        matrix, gcta_diag = lib.CalcAdjustedGRM(
            self.GENO, covariates, self.nThreads)
        return matrix, gcta_diag

    def CalcAdmixedGRM(self, pop_af, pop):
        matrix, gcta_diag = lib.CalcAdmixedGRM(
            self.GENO, pop_af, pop, self.nThreads)
        return matrix, gcta_diag

    def CalcGRMMatrix(self):
        af = self.CalcAlleleFrequencies()
        matrix = lib.CalcGRMMatrix(self.GENO, af, self.nSamples, self.nThreads)
        gcta_diag = lib.CalcGCTADiagonal(
            self.GENO, af, self.nSamples, self.nThreads)
        return matrix, gcta_diag

    def CalcIBSMatrix(self):
        return lib.CalcIBSMatrix(self.GENO, self.nSamples, self.nThreads)

    def CalcKINGMatrix(self):
        return lib.CalcKINGMatrix(self.GENO, self.nSamples, self.nThreads)

    def CalcUGRMMatrix(self):
        return lib.CalcUGRMMatrix(self.GENO, self.nSamples, self.nThreads)

    def FindUnrelated(self, threshold=0.044):
        king = self.CalcKINGMatrix()
        R = np.zeros((self.nSamples, self.nSamples))
        R[king > threshold] = 1
        r = np.sum(R, axis=0)

    # Ancestry Estimation

    def CalcPCAScores(self, nComponents):
        grm = self.CalcGRMMatrix()
        w, V = npl.eig(grm)
        ind = np.argsort(-w)
        return V[:, ind[:nComponents]]

    def CalcPCALoadingsExact(self, nComponents):
        af = self.CalcAlleleFrequencies()
        A = lib.UnpackGRMGeno(self.GENO, af, self.nSamples)
        U, s, _ = npl.svd(A.T, full_matrices=False)
        S = np.diag(s[:nComponents])
        U = U[:, :nComponents]
        return npl.solve(S, U.T)

    def ProjectPCA(self, ref_obj, loadings, nParts=10):
        nSNPsPart = self.nSNPs//nParts
        af = ref_obj.CalcAlleleFrequencies()
        nComponents = loadings.shape[0]
        scores = np.zeros((nComponents, self.nSamples))
        for i in range(nParts):
            A = lib.UnpackGRMGeno(
                self.GENO[:, i*nSNPsPart:(i+1)*nSNPsPart], af[i*nSNPsPart:(i+1)*nSNPsPart], self.nSamples)
            scores = scores + loadings[:, i*nSNPsPart:(i+1)*nSNPsPart]@A

        A = lib.UnpackGRMGeno(
            self.GENO[:, nParts*nSNPsPart:], af[nParts*nSNPsPart:], self.nSamples)
        scores = scores + loadings[:, nParts*nSNPsPart:]@A
        return scores.T

    def CalcSUGIBSScores(self, nComponents):
        ibs = self.CalcIBSMatrix()
        d = np.sum(ibs, axis=0)
        ugrm = self.CalcUGRMMatrix()
        D = np.diag(d**-0.5)
        I = D@ugrm@D
        w, V = npl.eig(I)
        ind = np.argsort(-w)
        return D@V[:, ind[1:nComponents+1]]

    def CalcSUGIBSLoadingsExact(self, nComponents):
        ibs = self.CalcIBSMatrix()
        d = np.sum(ibs, axis=0)
        D = np.diag(d**-0.5)
        A = lib.UnpackUGeno(self.GENO, self.nSamples)
        A = D@A.T
        U, s, _ = npl.svd(A, full_matrices=False)
        S = np.diag(s[1:nComponents+1])
        U = U[:, 1:nComponents+1]
        return npl.solve(S, U.T)

    def ProjectSUGIBS(self, ref_obj, loadings, nParts=10):
        nSNPsPart = self.nSNPs//nParts
        nComponents = loadings.shape[0]
        scores = np.zeros((nComponents, self.nSamples))
        for i in range(nParts):
            A = lib.UnpackUGeno(
                self.GENO[:, i*nSNPsPart:(i+1)*nSNPsPart], self.nSamples)
            scores = scores + loadings[:, i*nSNPsPart:(i+1)*nSNPsPart]@A

        A = lib.UnpackUGeno(
            self.GENO[:, nParts*nSNPsPart:], self.nSamples)
        scores = scores + loadings[:, nParts*nSNPsPart:]@A
        connect = CalcIBSConnection(ref_obj, self, self.nThreads)
        D = np.diag(connect**-1)
        scores = scores@D
        return scores.T