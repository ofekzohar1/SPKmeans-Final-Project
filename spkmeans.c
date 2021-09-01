#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define SQ(x) ((x)*(x))
#define EPSILON 1.0E-15
#define MAX_JACOBI_ITER 100
#define MAX_FEATURES 10
#define COMMA_CHAR ','
#define REQUIRED_NUM_OF_ARGUMENTS 4
#define K_ARGUMENT 1
#define GOAL_ARGUMENT 2
#define MAX_KMEANS_ITER 300
#define MAX_DATAPOINTS 50
#define END_OF_STRING '\0'
#define PRINT_FORMAT "%.4f"
#define ERROR_MSG "An Error Has Occurred\n"
#define INVALID_INPUT_MSG "Invalid Input!\n"

#define MyAssert(exp) \
if (!(exp)) {      \
fprintf(stderr, ERROR_MSG); \
assert (exp);   \
exit(EXIT_FAILURE);     \
}

#define FOREACH_GOAL(GOAL) \
GOAL(jacobi) \
GOAL(wam)  \
GOAL(ddg)   \
GOAL(lnorm)  \
GOAL(spk)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum {
    FOREACH_GOAL(GENERATE_ENUM)
    NUM_OF_GOALS
} GOAL;

static const char *GOAL_STRING[] = {FOREACH_GOAL(GENERATE_STRING)};

typedef struct {
    double *prevCentroid;
    double *currCentroid;
    int counter; /* Number of vectors (datapoints) in cluster */
} Cluster;

typedef struct {
    double value;
    double *vector;
} Eigenvalue;

double **fitClusteringToOriginData(int k, int dimension, int numOfVectors, double **vectorsArray, double **originData, Cluster *clustersArray);
Cluster *kMeans(int k, int maxIter, int dimension, int numOfVectors, double **vectorsArray, int *firstCentralIndexes);
Cluster *initClusters(double **vectorsArray, int k, int dimension, int *firstCentralIndexes);
double vectorsNorm(const double *vec1, const double *vec2, const int *dimension); /* Calculate the norm between 2 vectors */
int findMyCluster(double *vec, Cluster *clustersArray, const int *k, const int *dimension); /* Return the vector's closest cluster (in terms of norm) */
void assignVectorsToClusters(double **vectorsArray, Cluster *clustersArray, const int *k, const int *numOfVectors, const int *dimension); /* For any vector assign to his closest cluster */
int recalcCentroids(Cluster *clustersArray, const int *k, const int *dimension); /* Recalculate clusters' centroids, return number of changes */
void initCurrCentroidAndCounter(Cluster *clustersArray, const int *k, const int *dimension); /* Set curr centroid to prev centroid and reset the counter */
void freeMemoryVectorsClusters(double **vectorsArray, double **originData, Cluster *clustersArray, int k); /* Free the allocated memory */
void jacobiAlgorithm(double **matrix, double **eigenvectorsMat, int n);
void initIdentityMatrix(double **matrix, int n);
int eigengapHeuristicKCalc(Eigenvalue *eigenvalues, int n);
void **alloc2DArray(int rows, int cols, size_t basicSize, size_t basicPtrSize, void *freeUsedSpace);
void *myAlloc(void *usedMem, int len, size_t basicSize);
void merge(Eigenvalue arr[], int l, int m, int r, Eigenvalue L[], Eigenvalue R[]);
void mergeSort(Eigenvalue arr[], int l, int r, Eigenvalue L[], Eigenvalue R[]);
void printFinalCentroids(Cluster *clustersArray, int k, int dimension);
void printMatrix(double **matrix, int rows, int cols);
void validateAndAssignInput(int argc, char **argv, int *k, GOAL *goal, char **filenamePtr);
double **readDataFromFile(int *rows, int *cols, char *fileName, GOAL goal);
double **weightedMatrix(double **wMatrix, double **vectorsArray, int numOfVectors, int dimension);
double *dMatrix(double **wMatrix, int dim);
double **laplacian(double **wMatrix, double *dMatrix, int numOfVectors);
void calcDim(int *dimension, FILE *file, double *firstLine);
double **initTMatrix(Eigenvalue *eigenvalues, double **eigenvectorsMat, double **tMat, int n, int k);
Eigenvalue *sortEigenvalues(double **a, double **v, int n);
void printTest(double **a, double*v, int n);
void pivotIndex(double **matrix, int n, int *pivotRow, int *pivotCol);
void printJacobi(double **a, double **v, int n);

/**********************************
*********** Main ******************
**********************************/

int main(int argc, char *argv[]) {
    int k, dimension, numOfDatapoints;
    GOAL goal;
    char *filename;
    double **datapointsArray, **tMat, **wMat, **lnormMat, **eigenvectorsMat, *ddgMat, **freeUsedMem;
    Eigenvalue *eigenvalues;
    Cluster *clustersArray = NULL;
    /* int firstIndex[] = {44,46,68,64}; */

    validateAndAssignInput(argc, argv, &k, &goal, &filename);
    datapointsArray = readDataFromFile(&numOfDatapoints, &dimension, filename, goal);

    freeUsedMem = (double **) alloc2DArray(numOfDatapoints, numOfDatapoints, sizeof(double), sizeof(double *), NULL);
    if (goal == jacobi) {
        eigenvectorsMat = freeUsedMem;
        jacobiAlgorithm(datapointsArray, eigenvectorsMat, numOfDatapoints);
        printJacobi(datapointsArray, eigenvectorsMat, numOfDatapoints);
        goto end;
    }

    wMat = weightedMatrix(freeUsedMem, datapointsArray, numOfDatapoints, dimension);
    if (goal==wam){
        printMatrix(wMat, numOfDatapoints, numOfDatapoints);
        goto end;
    }

    ddgMat = dMatrix(wMat, numOfDatapoints);
    if (goal==ddg){
        printMatrix(&ddgMat, 1, numOfDatapoints);
        free(ddgMat);
        goto end;
    }

    lnormMat = laplacian(wMat, ddgMat, numOfDatapoints);
    if(goal==lnorm){
        printMatrix(lnormMat, numOfDatapoints, numOfDatapoints);
        goto end;
    }

    eigenvectorsMat = (double **) alloc2DArray(numOfDatapoints, numOfDatapoints, sizeof(double), sizeof(double *), NULL);
    jacobiAlgorithm(lnormMat, eigenvectorsMat, numOfDatapoints);
    eigenvalues = sortEigenvalues(lnormMat, eigenvectorsMat, numOfDatapoints);
    freeUsedMem = lnormMat;

    if (k == 0)
        k = eigengapHeuristicKCalc(eigenvalues, numOfDatapoints);
    tMat = initTMatrix(eigenvalues, eigenvectorsMat, freeUsedMem, numOfDatapoints, k);

    clustersArray = kMeans(k, MAX_KMEANS_ITER, k, numOfDatapoints, tMat, NULL);
    printFinalCentroids(clustersArray, k, k);
    freeUsedMem = tMat;

    end:
    freeMemoryVectorsClusters(freeUsedMem, datapointsArray, clustersArray, k);
    return 0;
}

void printJacobi(double **a, double **v, int n) {
    int i;
    
    for (i = 0; i < n; ++i) {
        if (i != 0)
            printf("%c", COMMA_CHAR);
        printf(PRINT_FORMAT, a[i][i]);
    }
    printf("\n");
    printMatrix(v, n, n);
}

void printTest(double **a, double*v, int n) {
    int i, j;

    FILE *file = fopen("..\\output3.txt", "w");
    for (i = 0; i < n; ++i) {
        fprintf(file, "%.4f,", a[i][i]);
    }
    fprintf(file, "\n\n");
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            if (j > 0)
                fprintf(file, "%c", COMMA_CHAR);
            fprintf(file, PRINT_FORMAT, v[j + i * n]);
        }
        fprintf(file, "\n");
    }
    fclose(file);
}

/**********************************
******** Spectral Clustering ******
**********************************/

double **dataClustering(int k, int maxIter, int dimension, int numOfVectors, double **vectorsArray, double **originData, int *firstCentralIndexes) {
    Cluster *clustersArray;
    double **originDataCentroids;

    clustersArray = kMeans(k, maxIter, k, numOfVectors, vectorsArray, firstCentralIndexes);
    originDataCentroids = fitClusteringToOriginData(k, dimension, numOfVectors, vectorsArray, originData, clustersArray);

    freeMemoryVectorsClusters(vectorsArray, originData, clustersArray, k);
    return originDataCentroids;
}

double **fitClusteringToOriginData(int k, int dimension, int numOfVectors, double **vectorsArray, double **originData, Cluster *clustersArray) {
    int i, j, clusterIndex, clusterSize;
    double **originDataCentroids;
    double *dataPoint;

    originDataCentroids = (double **) alloc2DArray(k, dimension, sizeof(double), sizeof(double *), NULL);
    for (i = 0; i < k; ++i) {
        for (j = 0; j < dimension; ++j) {
            originDataCentroids[i][j] = 0.0;
        }
    }

    for (i = 0; i < numOfVectors; ++i) {
        /* Set vectors final cluster to the corresponding original data */
        dataPoint = originData[i];
        clusterIndex = (int) vectorsArray[i][dimension];
        dataPoint[dimension] = clusterIndex;
        for (j = 0; j < dimension; ++j) {
            originDataCentroids[clusterIndex][j] += dataPoint[j];
        }
    }

    for (i = 0; i < k; ++i) {
        clusterSize = clustersArray[i].counter;
        for (j = 0; j < dimension; ++j) {
            originDataCentroids[i][j] /= clusterSize; /* Mean calc */
        }
    }
    return originDataCentroids;
}

void **alloc2DArray(int rows, int cols, size_t basicSize, size_t basicPtrSize, void *freeUsedSpace) {
    int i;
    void *blockMem, **matrix;
    /* Reallocate block of memory */
    blockMem = myAlloc(freeUsedSpace ,rows * cols, basicSize);
    matrix = myAlloc(NULL, rows, basicPtrSize);

    for (i = 0; i < rows; ++i) {
        matrix[i] = (void *) (((char *) blockMem) + i * cols * basicSize); /* Set matrix to point to 2nd dimension array */
    }
    return matrix;
}

void *myAlloc(void *usedMem, int len, size_t basicSize) {
    void *blockMem = realloc(usedMem, len * basicSize);
    MyAssert(blockMem != NULL);
    return blockMem;
}

void freeMemoryVectorsClusters(double **vectorsArray, double **originData, Cluster *clustersArray, int k) {
    int i;
    /* Free clusters */
    if (clustersArray != NULL) {
        for (i = 0; i < k; ++i) {
            free(clustersArray[i].currCentroid);
            free(clustersArray[i].prevCentroid);
        }
    }
    free(clustersArray);

    /* Free vectors */
    if (vectorsArray != NULL)
        free(*vectorsArray);
    free(vectorsArray);

    /* Free original data */
    if (originData != NULL)
        free(*originData);
    free(originData);
}

void printMatrix(double **matrix, int rows, int cols) {
    int i, j;

    for (i = 0; i < rows; ++i) {
        for (j = 0; j < cols; ++j) {
            if (j > 0)
                printf("%c", COMMA_CHAR);
            printf(PRINT_FORMAT, matrix[i][j]); /* Print with an accuracy of 4 digits after the dot */
        }
        printf("\n");
    }
}

GOAL str2enum(const char *str) {
    int j;
    for (j = 0; j < NUM_OF_GOALS; ++j) {
        if (!strcmp(str, GOAL_STRING[j]))
            return j;
    }
    return NUM_OF_GOALS; /* Invalid str to enum convert */
}

void printFinalCentroids(Cluster *clustersArray, int k, int dimension) {
    int i, j;
    for (i = 0; i < k; ++i) {
        for (j = 0; j < dimension; ++j) {
            if (j > 0)
                printf("%c", COMMA_CHAR);
            printf(PRINT_FORMAT, clustersArray[i].currCentroid[j]);
        }
        printf("\n");
    }
}

/*************************************
*********** kMeans Algorithm *********
*************************************/

Cluster *kMeans(int k, int maxIter, int dimension, int numOfVectors, double **vectorsArray, int *firstCentralIndexes) {
    int i, changes;
    Cluster *clustersArray;

    /* Initialize clusters arrays */
    clustersArray = initClusters(vectorsArray, k, dimension, firstCentralIndexes);

    for (i = 0; i < maxIter; ++i) {
        initCurrCentroidAndCounter(clustersArray, &k, &dimension); /* Update curr centroid to prev centroid and reset the counter */
        assignVectorsToClusters(vectorsArray, clustersArray, &k, &numOfVectors, &dimension);
        changes = recalcCentroids(clustersArray, &k, &dimension); /* Calculate new centroids */
        if (changes == 0) { /* Centroids stay unchanged in the current iteration */
            break;
        }
    }
    return clustersArray;
}

Cluster *initClusters(double **vectorsArray, int k, int dimension, int *firstCentralIndexes) {
    int i, j;
    Cluster *clustersArray;
    /* Allocate memory for clustersArray */
    clustersArray = (Cluster *) myAlloc(NULL, k, sizeof(Cluster));

    for (i = 0; i < k; ++i) {
        clustersArray[i].counter = 0;
        clustersArray[i].prevCentroid = (double *) myAlloc(NULL, dimension, sizeof(double));
        clustersArray[i].currCentroid = (double *) myAlloc(NULL, dimension, sizeof(double));

        if (firstCentralIndexes == NULL) { /* Kmeans */
            for (j = 0; j < dimension; ++j) {
                /* Assign the first k vectors to their corresponding clusters */
                clustersArray[i].currCentroid[j] = vectorsArray[i][j];
            }
        } else { /* kMeans++ */
            /* Assign the initial k vectors to their corresponding clusters according to the ones calculated in python */
            for (j = 0; j < dimension; ++j) {
                clustersArray[i].currCentroid[j] = vectorsArray[firstCentralIndexes[i]][j];
            }
        }
    }
    free(firstCentralIndexes); /* No longer necessary */
    return clustersArray;
}

double vectorsNorm(const double *vec1, const double *vec2, const int *dimension) {
    double norm = 0;
    int i;
    for (i = 0; i < *dimension; ++i) {
        norm += SQ(vec1[i] - vec2[i]);
    }
    return norm;
}

int findMyCluster(double *vec, Cluster *clustersArray, const int *k, const int *dimension) {
    int myCluster, j;
    double minNorm, norm;

    myCluster = 0;
    minNorm = vectorsNorm(vec, clustersArray[0].prevCentroid, dimension);
    for (j = 1; j < *k; ++j) { /* Find the min norm == the closest cluster */
        norm = vectorsNorm(vec, clustersArray[j].prevCentroid, dimension);
        if (norm < minNorm) {
            myCluster = j;
            minNorm = norm;
        }
    }
    return myCluster;
}

void assignVectorsToClusters(double **vectorsArray, Cluster *clustersArray, const int *k, const int *numOfVectors,
                             const int *dimension) {
    int i, j, myCluster;
    double *vec;
    for (i = 0; i < *numOfVectors; ++i) {
        vec = vectorsArray[i];
        myCluster = findMyCluster(vectorsArray[i], clustersArray, k, dimension);
        vec[*dimension] = myCluster; /* Set vector's cluster to his closest */
        for (j = 0; j < *dimension; ++j) {
            clustersArray[myCluster].currCentroid[j] += vec[j]; /* Summation of the vectors Components */
        }
        clustersArray[myCluster].counter++; /* Count the number of vectors for each cluster */
    }
}

int recalcCentroids(Cluster *clustersArray, const int *k, const int *dimension) {
    int i, j, changes = 0;
    Cluster cluster;
    for (i = 0; i < *k; ++i) {
        cluster = clustersArray[i];
        for (j = 0; j < *dimension; ++j) {
            cluster.currCentroid[j] /= cluster.counter; /* Calc the mean value */
            changes += cluster.prevCentroid[j] != cluster.currCentroid[j] ? 1
                    : 0; /* Count the number of changed centroids' components */
        }
    }
    return changes;
}

void initCurrCentroidAndCounter(Cluster *clustersArray, const int *k, const int *dimension) {
    int i, j;
    for (i = 0; i < *k; ++i) {
        for (j = 0; j < *dimension; ++j) {
            clustersArray[i].prevCentroid[j] = clustersArray[i].currCentroid[j]; /* Set prev centroid to curr centroid */
            clustersArray[i].currCentroid[j] = 0; /* Reset curr centroid */
        }
        clustersArray[i].counter = 0; /* Reset counter */
    }
}

/*************************************
*********** Jacobi Algorithm *********
*************************************/

void pivotIndex(double **matrix, int n, int *pivotRow, int *pivotCol) {
    int i, j;
    double maxAbs = -1, tempValue;
    for (i = 0; i < n; ++i) {
        for (j = i + 1; j < n; ++j) {
            tempValue = fabs(matrix[i][j]);
            if (maxAbs < tempValue) {
                maxAbs = tempValue;
                *pivotRow = i;
                *pivotCol = j;
            }
        }
    }
}

void initIdentityMatrix(double **matrix, int n) {
    int i, j;
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            matrix[i][j] = i == j ? 1.0 : 0.0;
        }
    }
}

double jacobiRotate(double **a, double **v, int n, int i, int j) {
    double theta, t, c, s;
    double ij, ii, jj, ir, jr;
    int r;

    theta = a[j][j] - a[i][i];
    theta /= (2 * a[i][j]);
    t = 1.0 / (fabs(theta) + sqrt(SQ(theta) + 1.0));
    t = theta < 0.0 ? -t : t;
    c = 1.0 / sqrt(SQ(t) + 1.0);
    s = t * c;

    ii = a[i][i];
    jj = a[j][j];
    ij = a[i][j];
    a[i][j] = 0.0;
    a[j][i] = 0.0;
    for (r = 0; r < n; r++) {
        if (r == i)
            /* c^2 * Aii + s^2 * Ajj - 2scAij */
            a[i][i] = SQ(c) * ii + SQ(s) * jj - 2 * s * c * ij;
        else if (r == j)
            /* s^2 * Aii + c^2 * Ajj + 2scAij */
            a[j][j] = SQ(s) * ii + SQ(c) * jj + 2 * s * c * ij;
        else { /* r != j, i */
            ir = a[i][r];
            jr = a[j][r];
            a[i][r] = c * ir - s * jr;
            a[j][r] = c * jr + s * ir;
            a[r][i] = a[i][r] ;
            a[r][j] = a[j][r] ;

        }

        /* Update the eigenvector matrix */
        ir = v[i][r];
        jr = v[j][r];
        v[i][r] = c * ir - s * jr;
        v[j][r] = c * jr + s * ir;
    }

    return 2 * SQ(ij); /* offNormDiff: Off(A)^2 - Off(A')^2 = 2 * Aij^2 */
}

Eigenvalue *sortEigenvalues(double **a, double **v, int n) {
    int i;
    Eigenvalue *L, *R, *eigenvalues = myAlloc(NULL, n, sizeof(Eigenvalue));

    for(i = 0; i < n; ++i) {
        eigenvalues[i].value = a[i][i];
        eigenvalues[i].vector = v[i];
    }

    L = (Eigenvalue *) myAlloc(NULL, 2 * n, sizeof(Eigenvalue));
    R = &L[n];
    mergeSort(eigenvalues, 0, n - 1, L, R);
    free(L);
    return eigenvalues;
}

void jacobiAlgorithm(double **matrix, double **eigenvectorsMat, int n) {
    double diffOffNorm;
    int jacobiIterCounter, pivotRow, pivotCol;

    initIdentityMatrix(eigenvectorsMat, n);

    jacobiIterCounter = 0;
    do {
        pivotIndex(matrix, n, &pivotRow, &pivotCol);
        diffOffNorm = jacobiRotate(matrix, eigenvectorsMat, n, pivotRow, pivotCol);
        jacobiIterCounter++;
    } while (jacobiIterCounter < MAX_JACOBI_ITER && diffOffNorm >= EPSILON);
}

int eigengapHeuristicKCalc(Eigenvalue *eigenvalues, int n) {
    int i, maxIndex, m;
    double maxDelta, delta;

    m = n / 2;
    maxDelta = -1;
    maxIndex = -1;
    for (i = 0; i < m; i++) {
        delta = eigenvalues[i + 1].value - eigenvalues[i].value;
        if (maxDelta < delta) {
            maxDelta = delta;
            maxIndex = i;
        }
    }

    return maxIndex + 1;
}

double **initTMatrix(Eigenvalue *eigenvalues, double **eigenvectorsMat, double **tMat, int n, int k) {
    int i, j;
    double sumSqRow, value;

    for (i = 0; i < n; ++i) {
        sumSqRow = 0.0;
        for (j = 0; j < k; ++j) {
            value = eigenvalues[j].vector[i];
            tMat[i][j] = value;
            sumSqRow += SQ(value);
        }
        if (sumSqRow != 0.0) { /* TODO check about zero line */
            sumSqRow = 1.0 / sqrt(sumSqRow);
            for (j = 0; j < k; ++j) {
                tMat[i][j] *= sumSqRow;
            }
        }
    }
    /* Free memory */
    free(eigenvalues);
    free(eigenvectorsMat[0]);
    free(eigenvectorsMat);
    return tMat;
}

/********* Ben's part *********/

/*
 * Builds an n*n matrix while n is the number of vectors
 * Assign the weighted factor as requested
 * Param - vectorsArray matrix, number of vectors and dimensions
 * Using sqrt and norm functions
 * Returns the weighted Matrix
 */
double **weightedMatrix(double **wMatrix, double **vectorsArray, int numOfVectors, int dimension) {
    int i, j;
    double norm;

    for (i = 0; i < numOfVectors; i++) {
        wMatrix[i][i] = 0.0;
        for (j = i + 1; j < numOfVectors; j++) {
            norm = sqrt(vectorsNorm(vectorsArray[i], vectorsArray[j], &dimension));
            wMatrix[i][j] = exp(-0.5 * norm);
            wMatrix[j][i] = wMatrix[i][j];
        }
    }
    return wMatrix;
}

/*
 *Build the Diagonal Degree Matrix from the Weighted Adjacency Matrix
 *returns a pointer to the Diagonal Degree Matrix
 * */
double *dMatrix(double **wMatrix, int dim) {
    int i, j;
    double *dMatrix, sum;
    dMatrix = (double *) myAlloc(NULL, dim, sizeof(double));

    for (i = 0; i < dim; i++) {
        sum = 0.0;
        for (j = 0; j < dim; j++) {
            sum += wMatrix[i][j];
        }
        dMatrix[i] = sum;
    }
    return dMatrix;
}

/*
 * Builds The Normalized Graph Laplacian from the weighted matrix and the degree matrix
 * returns a pointer to the Normalized Graph Laplacian Matrix*/
double **laplacian(double **wMatrix, double *dMatrix, int numOfVectors) {
    int i, j;
    double **lMatrix = wMatrix;

    for (i = 0; i < numOfVectors; i++) {
        dMatrix[i] = 1 / sqrt(dMatrix[i]);
    }

    for (i = 0; i < numOfVectors; i++) {
        for (j = 0; j < numOfVectors; j++) {
            lMatrix[i][j] = -1.0 * dMatrix[i] * dMatrix[j] * wMatrix[i][j];
            if (i == j)
                lMatrix[i][j] += 1.0;
        }
    }
    free(dMatrix); /* End of need */
    return lMatrix;
}

void calcDim(int *dimension, FILE *file, double *firstLine) {
    char c;
    double value;
    *dimension = 0;
    do {
        fscanf(file, "%lf%c", &value, &c);
        firstLine[(*dimension)++] = value;
    } while (c != '\n' && c != '\r');
}

double **readDataFromFile(int *rows, int *cols, char *fileName, GOAL goal) {
    int counter, maxLen;
    char c;
    double value;
    FILE *file;
    double **matrix, *dataBlock;

    maxLen = goal != jacobi ? MAX_FEATURES : MAX_DATAPOINTS;
    dataBlock = (double *) myAlloc(NULL, maxLen, sizeof(double));
    file = fopen(fileName, "r");
    MyAssert(file != NULL);
    calcDim(cols, file, dataBlock);

    maxLen = goal != jacobi ? MAX_DATAPOINTS : *cols;
    /* Reallocate memory to hold the data */
    dataBlock = (double *) myAlloc(dataBlock ,maxLen * (*cols), sizeof(double));

    counter = *cols;
    while (fscanf(file, "%lf%c", &value, &c) != EOF) {
        dataBlock[counter++] = value;
    }
    MyAssert(fclose(file) != EOF);

    *rows = counter / *cols;
    matrix = (double **) alloc2DArray(*rows, *cols, sizeof(double), sizeof(double *), dataBlock);
    return matrix;
}

void validateAndAssignInput(int argc, char **argv, int *k, GOAL *goal, char **filenamePtr) {
    char *nextCh;

    if (argc >= REQUIRED_NUM_OF_ARGUMENTS) {
        *k = strtol(argv[K_ARGUMENT], &nextCh, 10);
        *goal = str2enum(argv[GOAL_ARGUMENT]);
        *filenamePtr = argv[REQUIRED_NUM_OF_ARGUMENTS - 1];
        /* k greater than zero and the conversion succeeded, valid goal */
        if (*k >= 0 && *nextCh == END_OF_STRING && *goal < NUM_OF_GOALS)
            return;
    }
    printf(INVALID_INPUT_MSG);
    exit(0);
}

/*************************
 **** Merge Sort *********
 ************************/

/* Merge two Sub-arrays of arr[].
 First subarray is arr[l...m]
 Second subarray is arr[m+1..r] */
void merge(Eigenvalue arr[], int l, int m, int r, Eigenvalue L[], Eigenvalue R[])
{
    int i, j, k;
    int n1 = m - l + 1;
    int n2 = r - m;

    /* Copy data to temp arrays L[] and R[] */
    for (i = 0; i < n1; i++)
        L[i] = arr[l + i];
    for (j = 0; j < n2; j++)
        R[j] = arr[m + 1 + j];

    /* Merge the temp arrays back into arr[l...r]*/
    i = 0; /* Initial index of first subarray */
    j = 0; /* Initial index of second subarray */
    k = l; /* Initial index of merged subarray */
    while (i < n1 && j < n2) {
        if (L[i].value <= R[j].value) {
            arr[k] = L[i];
            i++;
        }
        else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    /* Copy the remaining elements of L[], if there
	are any */
    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }

    /* Copy the remaining elements of R[], if there
	are any */
    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }
}

/* l is for left index and r is right index of the
sub-array of arr to be sorted */
void mergeSort(Eigenvalue arr[], int l, int r, Eigenvalue L[], Eigenvalue R[])
{
    if (l < r) {
        /* Same as (l+r)/2, but avoids overflow for */
        /* large l and h */
        int m = l + (r - l) / 2;

        /* Sort first and second halves */
        mergeSort(arr, l, m, L, R);
        mergeSort(arr, m + 1, r, L, R);

        merge(arr, l, m, r, L, R);
    }
}



