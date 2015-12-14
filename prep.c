#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

#define BENCH 1

#if BENCH

#include <sys/time.h>

int dumpResults(struct timeval *t, long *l){
  time_t diff_sec = (t[1].tv_sec - t[0].tv_sec);
  suseconds_t diff_usec = (t[1].tv_usec - t[0].tv_usec);
  double diff = (double)diff_sec + ((double)diff_usec / 1000000);
  printf("# of data points : \t%ld of %ld\n", l[1], l[0]);
  printf("computation time : \t%.6f [sec.]\n", diff);
  printf("time per data point: \t%e [sec. / data point]\n", diff / l[1]);
  printf("time per whole data: \t%e [sec. / lines]\n", diff / l[0]);
  return 0;
}

#endif


/* genomic sequence */

long getFileSize(const char *fname){
  int fd;
  struct stat stbuf;

  if((fd = open(fname, O_RDONLY)) == -1){
    fprintf(stderr, "error: open %s\n%s\n", 
	    fname, strerror(errno));      
    exit(EXIT_FAILURE);
  }

  if(fstat(fd, &stbuf) == -1){
    fprintf(stderr, "error: fstat %s\n%s\n",
	    fname, strerror(errno));
    exit(EXIT_FAILURE);
  }

  close(fd);

  return stbuf.st_size;
}

int readFasta(const char *fastaName, 
	      char *sequenceHead,
	      char *sequence){
  FILE *fp;
  char buf[256];

  /* file open */
  if((fp = fopen(fastaName, "r")) == NULL){
    fprintf(stderr, "error: fdopen %s\n%s\n",
	    fastaName, strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* get sequence header */
  fscanf(fp, "%s", sequenceHead);
 
  /* get sequence body */
  while(fscanf(fp, "%s", buf) != EOF) {
    strncpy(sequence, buf, strlen(buf));
    sequence += strlen(buf);
  }

  fclose(fp);
  return 0;
}

inline int c2i(const char c){
  switch(c){
    case 'A':
    case 'a':
      return 0;
    case 'C':
    case 'c':
      return 1;
    case 'G':
    case 'g':
      return 2;
    case 'T':
    case 't':
      return 3;
   default:
     fprintf(stderr, "input genomic sequence contains unknown char : %c\n", c);
     exit(EXIT_FAILURE);
  }
}

int computeFeatureSub(const int k,
		      const int left,
		      const int right,
		      const char *sequence,
		      int **feature_i){
  int i, kmer = 0;
  const int bitMask = (1 << (2 * k)) - 1;

  /* check if the sequence contains N */
  for(i = left; i < right; i++){
    if(sequence[i] == 'N' || sequence[i] == 'n'){
      return -1;
    }
  }

  if((*feature_i = calloc(sizeof(int), 1 << (2 * k))) == NULL){
    perror("error(calloc) feature[i]");
    exit(EXIT_FAILURE);
  }
  
  for(i = left; i < left + k - 1; i++){
    kmer = (kmer << 2);
    kmer += c2i(sequence[i]);
  }

  /* count k-mer frequency */
  for(i = left + k - 1; i < right; i++){
    kmer = (kmer << 2);
    kmer += c2i(sequence[i]);
    kmer = kmer & bitMask;
    (*feature_i)[kmer] += 1;
  }

  return 0;
}
		       
int computeFeature(const int k,
		   const int binSize,
		   const char *sequence,
		   int ***feature,
		   int *binNum){
  int i;
  *binNum = strlen(sequence) / binSize;

  if((*feature = calloc(sizeof(int *), *binNum)) == NULL){
    perror("error(calloc) feature:");
    exit(EXIT_FAILURE);
  }
  
  for(i = 0; i < *binNum; i++){
    computeFeatureSub(k, i * binSize, (i + 1) * binSize + k, sequence, &((*feature)[i]));
  }
  return 0;
}

int sequencePrep(const int k,		 
		 const int binSize,
		 const char *fastaName, 
		 int ***feature,
		 int *binNum){
  /* parameters */
  char sequenceHead[80];
  char *sequence;
  long fileSize;

  /* allocate memory space for sequence */
  fileSize = getFileSize(fastaName);
  if((sequence = calloc(1, fileSize)) == NULL){
    perror("error(calloc) sequence:");
    exit(EXIT_FAILURE);
  }
  
  readFasta(fastaName, sequenceHead, sequence);	    
	    
  printf("loaded genome sequence %s (length = %ld)\n",
	 sequenceHead, (long)strlen(sequence));

  computeFeature(k, binSize, sequence, feature, binNum);

  free(sequence);

  return 0;
}

/* Hi-C contact frequency matrix  */

inline char *res2str(const int res){
  switch(res){
    case 1000:
      return "1kb";
    default:
      fprintf(stderr, "resolution size %d is not supported\n", res);
     exit(EXIT_FAILURE);
  }
}

int hicFileNames(const char *hicDir,
		 const int res,
		 const int chr,
		 const char *normalize,
		 const char *expected,
		 char **hicFileRaw,
		 char **hicFileNormalize,
		 char **hicFileExpected){
  char fileHead[100];

  if((*hicFileRaw = calloc(sizeof(char), 100)) == NULL){
    perror("error(calloc) hicFileRaw");
    exit(EXIT_FAILURE);
  }
  if((*hicFileNormalize = calloc(sizeof(char), 100)) == NULL){
    perror("error(calloc) hicFileNormalize");
    exit(EXIT_FAILURE);
  }
  if((*hicFileExpected = calloc(sizeof(char), 100)) == NULL){
    perror("error(calloc) hicFileExpected");
    exit(EXIT_FAILURE);
  }
  
  sprintf(fileHead, "%s%s_resolution_intrachromosomal/chr%d/MAPQGE30/chr%d_%s.",
	  hicDir, res2str(res), chr, chr, res2str(res));
    
  sprintf(*hicFileRaw, "%s%s", fileHead, "RAWobserved");

  if(normalize != NULL){
    sprintf(*hicFileNormalize, "%s%s%s", fileHead, normalize, "norm");
  }

  if(expected != NULL){
    sprintf(*hicFileExpected, "%s%s%s", fileHead, expected, "expected");
  }

  return 0;
}

int readDouble(const char *fileName, double **array, const int lineNum){
  FILE *fp;
  int i = 0;
  char buf[50];

  if((*array = calloc(sizeof(double), lineNum)) == NULL){
    perror("error(calloc) readDouble");
    exit(EXIT_FAILURE);
  }

  if((fp = fopen(fileName, "r")) == NULL){
    fprintf(stderr, "error: fopen %s\n%s\n",
	    fileName, strerror(errno));
    exit(EXIT_FAILURE);
  }

  while(fgets(buf, 50, fp) != NULL){
    if(i < lineNum){
      (*array)[i++] = strtod(buf, NULL);
    }else{
      break;
    }
  }

  fclose(fp);

  return 0;
}

int hicNormPrep(const char *hicDir, 
		const int res, 
		const int chr, 
		const long maxDist,
		const int binNum,
		const char *normalizeMethod,
		const char *expectedMethod,
		char **hicFileRaw,
		double **normalize, 
		double **expected){	    

  char *hicFileNormalize;
  char *hicFileExpected;

  hicFileNames(hicDir, res, chr, normalizeMethod, expectedMethod,
	       hicFileRaw,
	       &hicFileNormalize,
	       &hicFileExpected);

  if(normalizeMethod != NULL){
    readDouble(hicFileNormalize, normalize, binNum);
    free(hicFileNormalize);
  }

  if(expectedMethod != NULL){
    readDouble(hicFileExpected, expected, maxDist / res);
    free(hicFileExpected);
  }

  return 0;
}

/* normalize Hi-C data */

int HicPrep(const int k, 
	    const int res,
	    const int chr,
	    const char *normalizeMethod,
	    const char *expectedMethod,
	    const long minBinDist,
	    const long maxBinDist,
	    const int **feature, 
	    const char *hicFileRaw, 
	    const double *normalize,
	    const double *expected,
	    const char*outDir){
  FILE *fp, *fpOut;
  int i, j;
  double mij;
  char buf[100], mijbuf[20], outFile[100];

#if BENCH
  struct timeval tv[2];
  long benchLines[2];
#endif

  /* Hi-C input file open */
  if((fp = fopen(hicFileRaw, "r")) == NULL){
    fprintf(stderr, "error: fopen %s\n%s\n",
	    hicFileRaw, strerror(errno));
    exit(EXIT_FAILURE);
  }

  printf("Hi-C file %s is open\n", hicFileRaw);

  /* Hi-C output file open */
  sprintf(outFile, 
	  "%schr%d.m%ld.M%ld.%s.%s.dat", 
	  outDir, chr,
	  minBinDist * res, maxBinDist * res,
	  normalizeMethod, expectedMethod);

  if((fpOut = fopen(outFile, "w")) == NULL){
    fprintf(stderr, "error: fopen %s\n%s\n",
	    outFile, strerror(errno));
    exit(EXIT_FAILURE);
  }

  printf("Hi-C data file created : %s \n", outFile);

#if BENCH
  benchLines[0] = benchLines[1] = 0;
  if(gettimeofday(&(tv[0]), NULL) == -1){
    perror("gettimeofday");
    exit(EXIT_FAILURE);
  }
#endif

  /* normalize Hi-C data */
  while(fgets(buf, 100, fp) != NULL){

#if BENCH
    benchLines[0]++;
#endif

    sscanf(buf, "%d\t%d\t%s", &i, &j, (char *)(&mijbuf));
    i /= res;
    j /= res;
    if(minBinDist <= abs(i - j) && 
       abs(i - j) <= maxBinDist &&
       feature[i] != NULL &&
       feature[j] != NULL){

      /* normalize & O/E conversion */
      mij = strtod(mijbuf, NULL) / (normalize[i] * normalize[j] * expected[abs(i - j)]);

      if(!isnan(mij) && !isinf(mij)){

#if BENCH
	benchLines[1]++;
#endif
	
	fprintf(fpOut, "%d\t%d\t%f\n", i * res, j * res, mij);

      }
    }
  }

#if BENCH
  if(gettimeofday(&(tv[1]), NULL) == -1){
    perror("gettimeofday");
    exit(EXIT_FAILURE);
  }
#endif

  fclose(fpOut);
  fclose(fp);


#if BENCH
  dumpResults(tv, benchLines);
#endif


  return 0;
}

int featureSave(const char *outDir,
		const int k,
		const int res,
		const int chr,
		const int binNum,
		const int **feature){
  FILE *fp;
  long i, j, jMax;
  char fileName[100];

  jMax = (1 << (2 * k));

  sprintf(fileName, "%sk%d.res%d.chr%d.dat",
	  outDir, k, res, chr);


  if((fp = fopen(fileName, "w")) == NULL){
    fprintf(stderr, "error: fopen %s\n%s\n",
	    fileName, strerror(errno));
    exit(EXIT_FAILURE);
  }

  for(i = 0; i < binNum; i++){
    if(feature[i] == NULL){
      fprintf(fp, "*\n");
    }else{
      for(j = 0; j < jMax; j++){
	fprintf(fp, "%d\t", feature[i][j]);
      }
      fprintf(fp, "\n");
    }
  }
  
  fclose(fp);

  return 0;
}



int main_sub(const char *fastaName,
	     const char *hicDir,
	     const char *outDir,
	     const int k,
	     const int res,
	     const int chr,
	     const long minDist,
	     const long maxDist,
	     const char *normalizeMethod,
	     const char *expectedMethod){

  int **feature;
  char *hicFileRaw;
  double *normalize;
  double *expected;
  int binNum;
  int i;

  /* compute k-mer frequencies for each bin */
  sequencePrep(k, res, fastaName, &feature, &binNum);

  /* load Hi-C normalize vectors */
  hicNormPrep(hicDir, res, chr, maxDist, binNum,
	      normalizeMethod, expectedMethod,
	      &hicFileRaw, &normalize, &expected);

  /* normalize Hi-C data and write it into a file */
  HicPrep(k, res, chr, normalizeMethod, expectedMethod,
	  minDist / res, maxDist / res, 
	  (const int **)feature, hicFileRaw, 
	  normalize, expected, outDir);

  /* write feature vector */
  featureSave(outDir, k, res, chr, binNum, (const int **)feature);


  /* free the allocated memory and exit */
  free(normalize);
  free(expected);

  for(i = 0; i < binNum; i++){
    if(feature[i] != NULL){                
      free(feature[i]);
    }
  }
  
  free(feature);

  return 0;
}

int check_params(const char *fastaName,
		 const char *hicDir,
		 const char *outDir,
		 const int k,
		 const int res,
		 const int chr,
		 const long minDist,
		 const long maxDist,
		 const char *normalizeMethod,
		 const char *expectedMethod){
  if(fastaName == NULL){
    fprintf(stderr, "input fasta file is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("fasta file:    %s\n", fastaName);
  }
  if(hicDir == NULL){
    fprintf(stderr, "Hi-C data directory is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("Hi-C data dir: %s\n", hicDir);
  }
  if(outDir == NULL){
    fprintf(stderr, "output directory is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("Output dir:    %s\n", outDir);
  }
  if(k == -1){
    fprintf(stderr, "k is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("k: %d\n", k);
  }
  if(res == -1){
    fprintf(stderr, "resolution is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("resolution: %d\n", res);
  }
  if(chr == -1){
    fprintf(stderr, "chromosome number is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("chromosome: %d\n", chr);
  }
  if(minDist == -1){
    fprintf(stderr, "minimum distance is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("min distance: %ld\n", minDist);
  }
  if(maxDist == -1){
    fprintf(stderr, "maximum distance is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("Max distance: %ld\n", maxDist);
  }
  if(normalizeMethod == NULL){
    fprintf(stderr, "warning: normalize method is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("Normalization: %s\n", normalizeMethod);
  }
  if(expectedMethod == NULL){
    fprintf(stderr, "warning: expected value calculation method is not specified\n");
    exit(EXIT_FAILURE);
  }else{
    printf("Expectation:   %s\n", expectedMethod);
  }
  return 0;
}

int main(int argc, char **argv){
  char *fastaName = NULL; //"../data/GRCh37.ch21.fasta";
  char *hicDir = NULL; //"../data/GM12878_combined/";
  char *outDir = NULL; //"../out/";
  int k = -1; //1
  int res = -1; //1000;
  int chr = -1; //21;
  long minDist = -1; //10000;
  long maxDist = -1; //1000000;
  char *normalizeMethod = NULL; //"KR";
  char *expectedMethod = NULL; //"KR";

  struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    /* options */
    {"fasta", required_argument, NULL, 'f'},
    {"hic", required_argument, NULL, 'H'},
    {"out", required_argument, NULL, 'o'},
    {"k", required_argument, NULL, 'k'},
    {"res", required_argument, NULL, 'r'},
    {"chr", required_argument, NULL, 'c'},
    {"min", required_argument, NULL, 'm'},
    {"max", required_argument, NULL, 'M'},
    {"norm", required_argument, NULL, 'n'},
    {"expected", required_argument, NULL, 'e'},
    {0, 0, 0, 0}
  };

  int opt = 0;
  int opt_idx = 0;

  while((opt = getopt_long(argc, argv, "hvf:H:o:k:r:c:m:M:n:e:", long_opts, &opt_idx)) != -1){
    switch (opt){
      case 'h': /* help */
	printf("usage:\n\n");
	printf("example: ./chrom -f ../data/GRCh37.ch21.fasta -H ../data/GM12878_combined/ -o ../out/ -k1 -r1000 -c21 -m10000 -M1000000 -n\"KR\" -e\"KR\"\n");
	exit(EXIT_SUCCESS);
      case 'v': /* version*/
	printf("version: 0.10\n");
	exit(EXIT_SUCCESS);
      case 'f': /* fasta */
	fastaName = optarg;
	break;
      case 'H': /* hic */
	hicDir = optarg;
	break;
      case 'o': /* out */
	outDir = optarg;
	break;
      case 'k': /* k */
	k = atoi(optarg);
	break;
      case 'r': /* res */
	res = atoi(optarg);
	break;
      case 'c': /* chr */
	chr = atoi(optarg);
	break;
      case 'm': /* min */
	minDist = atol(optarg);
	break;
      case 'M': /* max */
	maxDist = atol(optarg);
	break;
      case 'n': /* norm */
	normalizeMethod = optarg;
	break;
      case 'e': /* expected */
	expectedMethod = optarg;
	break;
    }
  }

  check_params(fastaName, hicDir, outDir, k, res, chr, minDist, maxDist, normalizeMethod, expectedMethod);
  main_sub(fastaName, hicDir, outDir, k, res, chr, minDist, maxDist, normalizeMethod, expectedMethod);

  return 0;
}