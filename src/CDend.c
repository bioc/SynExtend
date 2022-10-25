#include "CDend.h"

/****** Callable Functions ******/
SEXP initCDend(SEXP dend){
  TREEHT = install("height");
  TREEMEM = install("members");
  TREELAB = install("label");
  TREELF = install("leaf");
  treeNode *head = convertRDend(dend);
  // I don't know why we have to add 1 here but otherwise it's always off
  head->value = labelTreePostorder(head, 0) + 1;
  SEXP retval = PROTECT(R_MakeExternalPtr(head, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(retval, (R_CFinalizer_t) FreeTree, TRUE);
  UNPROTECT(1);
  return retval;
}

/* Prints the tree
Unneeded, but keeping in for future reference
(since I know I'll forget how to do this later)
*/
SEXP printTree(SEXP tnPtr){
  treeNode *head = checkPtrExists(tnPtr);
  printHelper(head, 0);
  return R_NilValue;
}
//*/

SEXP getTreeNodesCount(SEXP tnPtr){
  treeNode *head = checkPtrExists(tnPtr);
  SEXP retval = PROTECT(allocVector(INTSXP, 1));
  INTEGER(retval)[0] = head->value+1;
  UNPROTECT(1);
  return retval;
}

SEXP hashString(SEXP label){
  SEXP ret = PROTECT(allocVector(INTSXP, 1));
  INTEGER(ret)[0] = hashLabel(STRING_ELT(label, 0));
  UNPROTECT(1);
  return ret;
}

SEXP calcGainLoss(SEXP tnPtr, SEXP occVec, SEXP convertToGL){
  treeNode *head = checkPtrExists(tnPtr);
  bool shouldConvert = LOGICAL(convertToGL)[0];
  int ovLen = LENGTH(occVec);
  unsigned int *presMap = malloc(sizeof(unsigned int) * ovLen);
  for (int i=0; i<ovLen; i++){
    presMap[i] = hashLabel(STRING_ELT(occVec, i));
  }

  //resetTree(head, 2);
  int lenOut = head->value + 1;
  int *PAvec = malloc(sizeof(int) * lenOut);
  fitchUp(head, presMap, ovLen, PAvec);
  free(presMap);

  fitchDown(head, 0, PAvec);
  fitchRecon(PAvec,  lenOut, 0);
  if (shouldConvert)
    convertGL(head, PAvec[head->value]==1, PAvec);

  SEXP retvec = PROTECT(allocVector(INTSXP, lenOut));
  int *rvPtr = INTEGER(retvec);
  for (int i = 0; i<lenOut; i++)
    rvPtr[i] = PAvec[i];
  //populateVector(head, rvPtr, 0);
  free(PAvec);

  UNPROTECT(1);
  return retvec;
}

SEXP calcScoreGL(SEXP tnPtr, SEXP glv1, SEXP glv2){
  treeNode *head = checkPtrExists(tnPtr);
  //if (LENGTH(glv1) != LENGTH(glv2)) error("Gain/Loss vectors are different lengths!");
  int *v1 = INTEGER(glv1);
  int *v2 = INTEGER(glv2);
  int numNodes = head->value + 1;
  double *nodeScores = malloc(sizeof(double) * numNodes);
  findNodeScores(head, v1, v2, nodeScores, head, true);

  // There is some numerical instability here
  long double r = 0.0;
  for ( int i=0; i < numNodes; i++){
    if (nodeScores[i] != 0){
       //r += (nodeScores[i] < 0 ? -1 : 1) / exp(fabs(nodeScores[i])-1);
      r += (nodeScores[i] < 0 ? -8.0 : 8.0) / (8.0*exp(fabsl(nodeScores[i])-1.0));
    }
  }

  free(nodeScores);
  SEXP retval = PROTECT(allocVector(REALSXP, 1));
  REAL(retval)[0] = r; 
  UNPROTECT(1);
  return retval;
}

SEXP calcScoreJaccard(SEXP ov1, SEXP ov2, SEXP NN){
  int v1len = INTEGER(NN)[0];
  int *v1 = INTEGER(ov1);
  int *v2 = INTEGER(ov2);

  double outval = 0.0;
  for (int i=0; i<v1len; i++)
    outval += v1[i] == v2[i];
  double denom = (double)(2*v1len) - outval;
  outval = outval / denom;

  SEXP retval = PROTECT(allocVector(REALSXP, 1));
  REAL(retval)[0] = outval;
  UNPROTECT(1);
  return retval;
}

SEXP calcScoreHamming(SEXP ov1, SEXP ov2, SEXP NN, SEXP norm){
  int v1len = INTEGER(NN)[0];
  double normer = REAL(norm)[0];
  int *v1 = INTEGER(ov1);
  int *v2 = INTEGER(ov2);

  double outval = 0.0;
  double addamt;
  for (int i=0; i<v1len; i++){
    addamt = abs(v1[i] - v2[i]);
    outval += addamt / normer;
  }
  outval = 1 - (outval / v1len);

  SEXP retval = PROTECT(allocVector(REALSXP, 1));
  REAL(retval)[0] = outval;
  UNPROTECT(1);
  return retval;
}

SEXP calcDValue(SEXP tnPtr, SEXP occVec){
  treeNode *head = checkPtrExists(tnPtr);

  int ovLen = LENGTH(occVec);
  unsigned int *presMap = malloc(sizeof(unsigned int) * ovLen);
  for (int i=0; i<ovLen; i++){
    presMap[i] = hashLabel(STRING_ELT(occVec, i));
  }

  double *scores = Calloc(head->value+1, double);
  calcSisterClades(head, presMap, ovLen, scores);

  double score = scoreSisterClades(head, scores);

  // cleanup
  SEXP retval = PROTECT(allocVector(REALSXP, 1));
  REAL(retval)[0] = score;
  Free(scores);
  UNPROTECT(1);

  return retval;
}

SEXP calcDRandValue(SEXP tnPtr, SEXP allLabels, SEXP numP, SEXP iterNum){
  // read in S expressions
  treeNode *head = checkPtrExists(tnPtr);

  int numPos = INTEGER(numP)[0];
  int numLabels = LENGTH(allLabels);
  int numIter = INTEGER(iterNum)[0];

  // get a vector of all the labels hashed
  unsigned int *allHashed = malloc(sizeof(unsigned int) * numLabels);
  for (int i=0; i<numLabels; i++){
    allHashed[i] = hashLabel(STRING_ELT(allLabels, i));
  }

  // seed random number generator
  GetRNGstate();

  int numScores = head->value+1;
  double *scores = Calloc(numScores, double);
  double randSum = 0.0;
  for (int i=0; i<numIter; i++){
    // reset vector
    memset(scores, 0, sizeof(double)*numScores);

    // get a random sample of choices for P/A vector
    shuffle(uint, allHashed, numLabels);
    calcSisterClades(head, allHashed, numPos, scores);
    randSum += scoreSisterClades(head, scores);
  }
  randSum /= numIter;

  // cleanup
  SEXP retval = PROTECT(allocVector(REALSXP, 1));
  REAL(retval)[0] = randSum;
  Free(scores);
  free(allHashed);
  PutRNGstate();
  UNPROTECT(1);
  return retval;
}

SEXP calcDBrownValue(SEXP tnPtr, SEXP allLabels, SEXP iterNum, SEXP SD, SEXP START, SEXP THRESH){
  // read in SEXPs
  treeNode *head = checkPtrExists(tnPtr);

  int numLabels = LENGTH(allLabels);
  int numIter = INTEGER(iterNum)[0];
  double sd = REAL(SD)[0];
  double startval = REAL(START)[0];
  double threshold = REAL(THRESH)[0];

  // get a vector of all the labels hashed
  unsigned int *allHashed = malloc(sizeof(unsigned int) * numLabels);
  for (int i=0; i<numLabels; i++){
    allHashed[i] = hashLabel(STRING_ELT(allLabels, i));
  }

  int *hashMapping = malloc(sizeof(int) * numLabels);
  findMapping(head, hashMapping, allHashed, numLabels);

  // seed random number generator
  GetRNGstate();

  int numScores = head->value+1;
  double *scores = Calloc(numScores, double);
  unsigned int *labs = Calloc(numLabels, unsigned int);
  double randSum = 0.0;
  int ctr;
  for (int i=0; i<numIter; i++){
    // reset vector
    memset(scores, 0, sizeof(double)*numScores);
    ctr = 0;

    // propagate brownian evolution
    propBrownianEvo(head, scores, startval, sd);

    // convert evoscores to a P/A vector
    for (int j=0; j<numScores; j++){
      if (scores[j] > threshold){
        for (int k=0; k<numLabels; k++){
          if (hashMapping[k] == j){
            labs[ctr] = allHashed[hashMapping[k]];
            ctr++;
            break;
          }
        }
      }
    }

    memset(scores, 0, sizeof(double)*numScores);
    calcSisterClades(head, labs, ctr, scores);
    randSum += scoreSisterClades(head, scores);
  }

  randSum /= numIter;

  // cleanup
  SEXP retval = PROTECT(allocVector(REALSXP, 1));
  REAL(retval)[0] = randSum;
  Free(scores);
  Free(labs);
  free(allHashed);
  PutRNGstate();
  UNPROTECT(1);
  return retval;
}

/******  D value internal calculations  ******/
void calcSisterClades(treeNode *node, unsigned int *pmap, int pmaplen, double *scoreArr){
  int nv = node->value;
  if (node->label != 0){
    // leaf node
    bool found = false;
    for (int i = 0; i<pmaplen; i++){
      if (node->label == pmap[i]){
        found = true;
        break;
      }
    }

    scoreArr[nv] = found ? 1.0 : 0.0;
    return;
  }

  // if we get here, it's not a leaf node
  double ch = node->height;
  double d;
  double lscore = 0.0;
  if (node->left){
    calcSisterClades(node->left, pmap, pmaplen, scoreArr);
    d = ch - node->left->height;
    lscore = d == 0 ? d : scoreArr[node->left->value] / d;
  }

  double rscore = 0.0;
  if (node->right){
    calcSisterClades(node->right, pmap, pmaplen, scoreArr);
    d = ch - node->right->height;
    rscore = d == 0 ? d : scoreArr[node->right->value] / d;
  }

  double curscore = (lscore + rscore) / 2.0;
  scoreArr[nv] = curscore;

  return;
}

double scoreSisterClades(treeNode *node, double *scores){
  double retval = 0.0;

  if (node->label != 0)
    return retval;

  // add children
  retval += scoreSisterClades(node->left, scores);
  retval += scoreSisterClades(node->right, scores);
  
  double curscore = scores[node->value];
  // add current node
  retval += fabsl(curscore - scores[node->left->value]);
  retval += fabsl(curscore - scores[node->right->value]);

  return retval;
}

void propBrownianEvo(treeNode *node, double *scores, double curval, double sd){
  scores[node->value] = curval;
  double h = node->height;
  double offset, mult;
  if (node->left){
    mult = fabs(h - node->left->height);
    offset = rnorm(0, sd*mult);
    propBrownianEvo(node->left, scores, curval+offset, sd);
  }

  if (node->right){
    mult = fabs(h - node->left->height);
    offset = rnorm(0, sd*mult);
    propBrownianEvo(node->right, scores, curval+offset, sd);
  }
}

void findMapping(treeNode *node, int *mapping, unsigned int *hashvals, int lenHash){
  if (node->label != 0){
    unsigned int lab = node->label;
    for (int i=0; i<lenHash; i++){
      if (hashvals[i] == lab){
        mapping[i] = node->value;
        return;
      }
    }
  }

  if (node->left) findMapping(node->left, mapping, hashvals, lenHash);
  if (node->right) findMapping(node->right, mapping, hashvals, lenHash);
}


/****** Gain / Loss Functions ******/
void findNodeScores(treeNode *curNode, int *v1, int *v2, double *scores, treeNode* head, bool isHead){
  int v = curNode->value;
  scores[v] = 0.0;
  if (v1[v] != 0){
    treeNode *ssNode=NULL, *osNode=NULL;
    bool sameVal, useOS=false;
    double h;

    ssNode = findNextNode(curNode, v2, v1, true);
    
    if (ssNode){
      double mpOS, mpSS, mpCN, oshd;

      // midpoint of found node
      mpCN = curNode->left ? curNode->left->height : 0;
      mpCN = (curNode->height + mpCN) / 2;

      // midpoint of sameside node
      mpSS = ssNode->left ? ssNode->left->height : 0;
      mpSS = (ssNode->height + mpSS) / 2;


      // check otherside
      if (!isHead){
        treeNode *otherSideStart = v <= head->left->value ? head->right : head->left;
        osNode = findNextNode(otherSideStart, v2, v1, false);
      }

      // midpoint of otherside node
      if (osNode){
        mpOS = osNode->left ? osNode->left->height : 0;
        mpOS = (osNode->height + mpOS) / 2;
        oshd = (2*head->height - mpOS - mpCN);
      }
      
      if (osNode && ssNode)
        useOS = oshd < (mpCN - mpSS);

      // just for simplicity
      ssNode = useOS ? osNode : ssNode;

      sameVal = v1[v] == v2[ssNode->value];
      if (useOS)
        h = 2*head->height - mpOS - mpCN;
      else{
        if (mpSS == mpCN){
          h = curNode->left ? curNode->left->height : 0.0;
          h = (curNode->height - h) / 3;
        } else {
          h = fabs(mpCN - mpSS);
        }
      } 

      h++;
      scores[v] = (sameVal ? 1 : -1) * h;
    }
  }

  if (curNode->left) findNodeScores(curNode->left, v1, v2, scores, head, false);
  if (curNode->right) findNodeScores(curNode->right, v1, v2, scores, head, false);

  return;
}

treeNode* findNextNode(treeNode *curNode, int *v, int *selfv, bool isCur){
  if (!curNode || (!isCur && selfv[curNode->value] != 0)){
    return NULL;
  }
  if (v[curNode->value] != 0)
    return curNode;


  treeNode* leftnode = NULL;
  if (curNode->left)
    leftnode = findNextNode(curNode->left, v, selfv, false);

  treeNode* rightnode = NULL;
  if (curNode->right)
    rightnode = findNextNode(curNode->right, v, selfv, false);

  if (rightnode && leftnode){
    double rmp = rightnode->left ? (rightnode->height + rightnode->left->height) : rightnode->height;
    double lmp = leftnode->left ? (leftnode->height + leftnode->left->height) : rightnode->height;
    return rmp < lmp ? rightnode : leftnode;
  }
  return rightnode ? rightnode : leftnode;
}


/****** Fitch Parsimony Functions ******/

void resetTree(treeNode* node, int val){
  node->value = val;
  if (node->left) resetTree(node->left, val);
  if (node->right) resetTree(node->right, val);
}

// Up Phase, starts with leaves and propagates up
void fitchUp(treeNode* node, unsigned int* hashMap, int hashMapLen, int *PAvec){
  int nv = node->value;
  if (node->label != 0){
    // leaf node
    bool found = false;
    for (int i = 0; i<hashMapLen; i++){
      if (node->label == hashMap[i]){
        found = true;
        break;
      }
    }

    PAvec[nv] = found ? 1 : 0;
    return;
  }

  // non-leaf node
  int lv=2, rv=2;
  if (node->left){
    fitchUp(node->left, hashMap, hashMapLen, PAvec);
    lv = PAvec[node->left->value];
  }

  if (node->right){
    fitchUp(node->right, hashMap, hashMapLen, PAvec);
    rv = PAvec[node->right->value];
  }

  // assign value based on children
  if (rv == 2 || lv == 2){
    PAvec[nv] = rv==2 ? lv : rv;
  } else {
    PAvec[nv] = rv==lv ? rv : 2;
  }

  return;
}

// Down phase, takes consensus between parent and children
void fitchDown(treeNode* node, int parentVal, int *PAvec){
  if (node->label != 0) return;
  if (PAvec[node->value] == 2){
    int counts[3] = {0, 0, 0};
    counts[parentVal]++;

    int lv = node->left ? PAvec[node->left->value] : 2;
    int rv = node->right ? PAvec[node->right->value] : 2;
    counts[lv]++;
    counts[rv]++;

    int endval = 2;
    if (counts[2] != 3 && counts[0] != counts[1])
      endval = counts[1] > counts[0] ? 1 : 0;

    PAvec[node->value] = endval;
  }
  int val = PAvec[node->value];
  if (node->left) fitchDown(node->left, val, PAvec);
  if (node->right) fitchDown(node->right, val, PAvec);

  return;
}

// Reconciliation, any nodes we couldn't set we just set to defaultVal
void fitchRecon(int* PAvec, int len, int defaultVal){
  for (int i=0; i<len; i++)
    if (PAvec[i] == 2) PAvec[i] = defaultVal;
  return;
}

void convertGL(treeNode* node, bool curVal, int *PAvec){
  if ((PAvec[node->value]==1) ^ curVal){
    // if curVal was true and we've changed, it's a loss (else gain)
    PAvec[node->value] = curVal ? -1 : 1;
    curVal = !curVal;
  } else {
    PAvec[node->value] = 0;
  }

  if (node->left) convertGL(node->left, curVal, PAvec);
  if (node->right) convertGL(node->right, curVal, PAvec);
  return;
}

int populateVector(treeNode* node, int *container, int idx){
  int curIdx = idx;
  container[curIdx] = node->value;
  curIdx++;
  if (node->left) curIdx = populateVector(node->left, container, curIdx);
  if (node->right) curIdx = populateVector(node->right, container, curIdx);
  
  return curIdx;
}

/****** Allocation Functions ******/

treeNode *allocTreeNode(double h, int v, int m, unsigned int l){
  treeNode *newNode = Calloc(1, treeNode);
  newNode->left = NULL;
  newNode->right = NULL;
  newNode->height = h;
  newNode->value = v;
  newNode->members = m;
  newNode->label = l;
  return newNode;
}

/****** Conversion Functions ******/

// convert dendrogram to treeNode
treeNode *convertRDend(SEXP dend){
  double h = 0.0;
  int v = -1;
  int m = 1;
  unsigned int label = 0;

  if (!isNull(getAttrib(dend, TREEHT)))
    h = REAL(getAttrib(dend, TREEHT))[0];

  if (!isNull(getAttrib(dend, TREEMEM)))
    m = INTEGER(getAttrib(dend, TREEMEM))[0];

  if (!isNull(getAttrib(dend, TREELAB)))
      label = hashLabel(STRING_ELT(getAttrib(dend, TREELAB), 0));

  if (!isNull(getAttrib(dend, TREELF))){
    return allocTreeNode(h, v, m, label);
  }

  treeNode *cur = allocTreeNode(h, v, m, label);
  cur->left = convertRDend(VECTOR_ELT(dend, 0));
  cur->right = convertRDend(VECTOR_ELT(dend, 1));

  return cur;
}

unsigned int hashLabel(SEXP label){
  const char *s = Rf_translateCharUTF8(label);

  unsigned int result = 0x55555555;

  while (*s) { 
      result ^= *s++;
      result = ((result << 5) | (result >> (27)));
  }

  return result;
}

/****** Miscellaneous ******/

static inline int getNumNodes(treeNode* node, int n){
  int numNodes = n;
  if (node->left) numNodes = getNumNodes(node->left, numNodes);
  if (node->right) numNodes = getNumNodes(node->right, numNodes);
  numNodes++;
  node->value = numNodes;
  return numNodes;
}

static inline int labelTreePostorder(treeNode* node, int n){
  // unsure why this has to be -1 as well, there's an off by one error otherwise
  int numNodes = n-1;
  if (node->left) numNodes = getNumNodes(node->left, numNodes);
  if (node->right) numNodes = getNumNodes(node->right, numNodes);
  node->value = numNodes;

  return numNodes++;
}

///* See comment on printTree
// helper function to print tree (in order)
static void printHelper(treeNode* node, int depth){
  if (!node) return;
  int vl = node->left ? node->left->value : 0;
  int vr = node->right ? node->right->value : 0;
  if (node->label != 0) Rprintf("LEAF %u (value %d)", node->label, node->value);
  else {
    Rprintf("%f (v: %d, l: %d, r: %d)", 
              node->height, node->value, vl, vr);
  }
  Rprintf("\n");
  if (node->left)
    printHelper(node->left, depth+1);
  if (node->right)
    printHelper(node->right, depth+1);

  return;
}
//*/

static void FreeTree(SEXP tnPtr){
  if (!R_ExternalPtrAddr(tnPtr)) return;
  treeNode *head = (treeNode *) R_ExternalPtrAddr(tnPtr);
  CleanupTree(head);
  R_ClearExternalPtr(tnPtr);
  return;
}

static inline treeNode* checkPtrExists(SEXP tnPtr){
  if (!R_ExternalPtrAddr(tnPtr))
    error("External pointer no longer exists!");
  return (treeNode *) R_ExternalPtrAddr(tnPtr);
}

// helper function to free memory
static void CleanupTree(treeNode* head){
  if (!head) return;
  CleanupTree(head->left);
  CleanupTree(head->right);
  Free(head);

  return;
}