###### Presence/Absence Methods for EvoWeaver #####
# author: Aidan Lakshman
# contact: ahl27@pitt.edu

#### Implemented Methods: ####
#  - Jaccard Distance at tips
#  - Hamming Distance
#  - Mutual Information
#  - Inverse Potts Model
#  - Behdenna 2016 gain/loss
#  - Pearson correlation of gain/loss (w/ p-value)
#  - Distance between gain/loss events
##########################

#### S3 Generic Definitions ####
Jaccard <- function(ew, ...) UseMethod('Jaccard')
Hamming <- function(ew, ...) UseMethod('Hamming')
CorrGL <- function(ew, ...) UseMethod('CorrGL')
MutualInformation <- function(ew, ...) UseMethod('MutualInformation')
ProfileDCA <- function(ew, ...) UseMethod('ProfileDCA')
Behdenna <- function(ew, ...) UseMethod('Behdenna')
GainLoss <- function(ew, ...) UseMethod('GainLoss')
PAPV <- function(ew, ...) UseMethod('PAPV')
################################

Jaccard.EvoWeaver <- function(ew, Subset=NULL, Verbose=TRUE,
                               precalcProfs=NULL, precalcSubset=NULL, ...){
  
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  evalmap <- subs$evalmap
  
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, uvals, Verbose=Verbose)
  } else {
    pap <- precalcProfs
  }
  l <- length(uvals)
  n <- names(ew)
  if ( l == 1 ){
    mat <- matrix(1, nrow=1, ncol=1)
    rownames(mat) <- colnames(mat) <- n
    return(mat)
  }
  nr <- nrow(pap)
  pap[] <- as.integer(pap) 
  ARGS <- list(nr=nr)
  FXN <- function(v1, v2, ARGS, ii, jj) {
    return(.Call("calcScoreJaccard", v1, v2, ARGS$nr, PACKAGE="SynExtend"))
  }
  pairscores <- BuildSimMatInternal(pap, uvals, evalmap, l, n, FXN, ARGS, Verbose)

  return(pairscores)
}

Hamming.EvoWeaver <- function(ew, Subset=NULL, Verbose=TRUE,
                               precalcProfs=NULL, precalcSubset=NULL, ...){
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  evalmap <- subs$evalmap
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, Verbose=Verbose)
  } else {
    pap <- precalcProfs
  }
  l <- length(uvals)
  n <- names(ew)
  if ( l == 1 ){
    mat <- simMat(1, nelem=1)
    rownames(mat) <- colnames(mat) <- n
    return(mat)
  }
  nr <- nrow(pap)
  pairscores <- rep(NA_real_, l*(l-1)/2)
  #nc <- ncol(pap)
  pap[] <- as.integer(pap)
  ARGS <- list(nr=nr)
  FXN <- function(v1, v2, ARGS, ii, jj) {
    return(.Call("calcScoreHamming", v1, v2, ARGS$nr, 1, PACKAGE="SynExtend"))
  }
  pairscores <- BuildSimMatInternal(pap, uvals, evalmap, l, n, FXN, ARGS, Verbose)
  
  return(pairscores)
}

CorrGL.EvoWeaver <- function(ew, Subset=NULL, Verbose=TRUE, 
                                MySpeciesTree=NULL,
                               precalcProfs=NULL, precalcSubset=NULL, ...){
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  evalmap <- subs$evalmap
  if (is.null(MySpeciesTree)){
    stopifnot("Method 'GainLoss' requires a species tree"=attr(ew, 'useMT'))
    if (Verbose) cat('Calculating Species Tree...\n')
    MySpeciesTree <- findSpeciesTree(ew, Verbose)
  }
  
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, uvals, Verbose=Verbose, speciesList=labels(MySpeciesTree), ...)
  } else {
    pap <- precalcProfs
  }
  l <- length(uvals)
  n <- names(ew)
  
  if ( l == 1 ){
    mat <- simMat(1, nelem=1)
    rownames(mat) <- colnames(mat) <- n
    return(mat)
  }
  # Initialize Dendrogram in C
  y <- .Call("initCDend", MySpeciesTree, PACKAGE="SynExtend")
  on.exit(rm(y))
  numnodes <- .Call("getTreeNodesCount", y, PACKAGE="SynExtend")
  rn <- rownames(pap)
  glvs <- matrix(NA_integer_, nrow=numnodes, ncol=l)
  
  # Calculate Gain/Loss Vectors
  if (Verbose) cat('  Calculating gain/loss vectors:\n')
  if (Verbose) pb <- txtProgressBar(max=ncol(pap), style=3)
  for (i in seq_len(l)){
    v <- rn[pap[,i]]
    if (length(v) == 0){
      glv <- rep(0L, numnodes)
    } else {
      glv <- .Call("calcGainLoss", y, v, TRUE, PACKAGE="SynExtend")
    }
    glvs[,i] <- glv
    if (Verbose) setTxtProgressBar(pb, i)
  }
  if(Verbose) cat("\n")
  
  ARGS <- list(numnodes=numnodes)
  FXN <- function(v1, v2, ARGS, ii, jj) {
    val <- cor(v1, v2)
    #pval <- 1 - pt(val, ARGS$numnodes - 2, lower.tail=FALSE)
    pval <- 1-fisher.test(v1, v2, simulate.p.value=TRUE)$p.value
    return(pval*val)
  }
  pairscores <- BuildSimMatInternal(glvs, uvals, evalmap, l, n, FXN, ARGS, Verbose)
  
  return(pairscores)
}


MutualInformation.EvoWeaver <- function(ew, Subset=NULL, Verbose=TRUE, 
                                         precalcProfs=NULL, precalcSubset=NULL, ...){
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  evalmap <- subs$evalmap
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, uvals, Verbose=Verbose)
  } else {
    pap <- precalcProfs
  }
  l <- length(uvals)
  n <- names(ew)
  if ( l == 1 ){
    mat <- matrix(1, nrow=1, ncol=1)
    rownames(mat) <- colnames(mat) <- n
    return(mat)
  }
  
  FXN <- function(v1, v2, ARGS, ii, jj){
    score <- 0
    v1l <- length(v1)
    tt <- sum(v1 & v2) / v1l
    tf <- sum(v1 & !v2) / v1l
    ft <- sum(!v1 & v2) / v1l
    ff <- sum(!v1 & !v2) / v1l
    jpd <- c(tt, tf, ft, ff)
    
    tv1 <- sum(v1) / v1l
    tv2 <- sum(v2) / v1l
    fv1 <- 1 - tv1
    fv2 <- 1 - tv2
    mpdv1 <- c(tv1, tv1, fv1, fv1)
    mpdv2 <- c(tv2, fv2, tv2, fv2)
    
    mult <- c(1,-1,-1,1)
    
    for ( k in seq_along(jpd) ){
      val <- jpd[k] * log(jpd[k] / (mpdv1[k] * mpdv2[k]), base=2) * mult[k]
      score <- score + ifelse(is.nan(val), 0, val)
    }
    return(score)
  }
  
  # APC Correction, removing for now
  # CORRECTION <- function(ps){
  #   apccorr <- mean(ps, na.rm=TRUE)
  #   ps <- ps - apccorr
  #   ps <- abs(ps)
  #   # Normalize
  #   denom <- max(ps, na.rm=TRUE)
  #   ps <- ps / ifelse(denom==0, 1, denom)
  # }
  pairscores <- BuildSimMatInternal(pap, uvals, evalmap, l, n, 
                                    FXN, NULL, Verbose)
                                    #CORRECTION=CORRECTION)
  
  Diag(pairscores) <- 1
  #pairscores <- pairscores #because distance
  return(pairscores)
}

ProfileDCA.EvoWeaver <- function(ew, Subset=NULL, Verbose=TRUE, Processors=1L,
                                  precalcProfs=NULL, precalcSubset=NULL, useAbs=TRUE, ...){
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, uvals, Verbose=Verbose)
  } else {
    pap <- precalcProfs
  }
  l <- ncol(pap)
  n <- colnames(pap)
  if ( l == 1 ){
    mat <- matrix(1, nrow=1, ncol=1)
    rownames(mat) <- colnames(mat) <- n
    return(mat)
  }
  
  pairscores <- DCA_logRISE(pap, Verbose=Verbose, Processors=Processors, ...)
  rownames(pairscores) <- colnames(pairscores) <- n
  pairscores <- as.simMat(pairscores)
  if (useAbs) pairscores <- abs(pairscores)
  if (max(pairscores) != 0)
    pairscores <- pairscores / max(abs(pairscores))
  
  return(pairscores)
}


Behdenna.EvoWeaver <- function(ew, Subset=NULL, Verbose=TRUE, 
                                MySpeciesTree=NULL, useSubtree=FALSE, 
                                useACCTRAN=TRUE, rawZScores=FALSE, 
                                precalcProfs=NULL, precalcSubset=NULL, ...){
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  evalmap <- subs$evalmap
  
  if (is.null(MySpeciesTree)){
      stopifnot("Method 'Behdenna' requires a species tree"=attr(ew, 'useMT'))
      if (Verbose) cat('Calculating Species Tree...\n')
      MySpeciesTree <- findSpeciesTree(ew, Verbose)
  }
  
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, uvals, Verbose=Verbose, speciesList=labels(MySpeciesTree))
  } else {
    pap <- precalcProfs
  }
  l <- length(uvals)
  stopifnot(nrow(pap) > 1)
  fd <- FastDend(MySpeciesTree)
  v1 <- abs(generateGainLossVec(fd, pap[,1], moveEventsUpward=useACCTRAN))
  glmat <- matrix(0, nrow=length(v1), ncol=ncol(pap))
  glmat[,1] <- v1
  if (Verbose) cat('  Calculating gain/loss vectors:\n')
  if (Verbose) pb <- txtProgressBar(max=ncol(pap), style=3)
  for ( i in 2:ncol(pap) ){
    glmat[,i] <- abs(generateGainLossVec(fd, pap[,i], moveEventsUpward=useACCTRAN))
    if (Verbose) setTxtProgressBar(pb, i)
  }
  if (Verbose) cat('\n')
  vals <- calc_SId_mat(fd, IdOnly=!useSubtree)
  if (useSubtree)
    M <- vals$S
  else
    M <- vals$Id
  Cmat <- vals$Cmat
  bl <- vals$blengths
  
  glmat <- abs(glmat)
  
  ARGS <- list(M=M, Cmat=Cmat, bl=bl)
  FXN <- function(v1, v2, ARGS, ii, jj){
    n1 <- sum(v1)
    n2 <- sum(v2)
    score <- 0
    if ( n1*n2 != 0 ){
      score <- t(v1) %*% ARGS$M %*% v2
      exp_mean <- n2 * (t(v1) %*% ARGS$M %*% ARGS$bl)
      exp_var <- n2*t(v1) %*% ARGS$M %*% ARGS$Cmat %*% t(M) %*% v1
      score <- (score - exp_mean) / sqrt(abs(exp_var))
    }
    
    return(score)
  }
  CORRECTION <- NULL
  if (!rawZScores){
    CORRECTION <- function(ps){
      ps <- abs(ps)
      ps <- ps / ifelse(max(ps,na.rm=TRUE) != 0, 
                        max(ps, na.rm=TRUE), 1)
      return(ps)
    }
  }
  pairscores <- BuildSimMatInternal(glmat, uvals, evalmap, l, names(ew), 
                                    FXN, ARGS, Verbose,
                                    CORRECTION=CORRECTION)
  
  Diag(pairscores) <- ifelse(rawZScores, 1, 0)
  
  return(pairscores)
}

GainLoss.EvoWeaver <- function(ew, Subset=NULL, 
                     Verbose=TRUE, MySpeciesTree=NULL, 
                     precalcProfs=NULL, precalcSubset=NULL, ...){
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  evalmap <- subs$evalmap
  BOOTSTRAP_NUM <- 100L

  if (is.null(MySpeciesTree)){
    stopifnot("Method 'GainLoss' requires a species tree"=attr(ew, 'useMT'))
    if (Verbose) cat('Calculating Species Tree...\n')
    MySpeciesTree <- findSpeciesTree(ew, Verbose)
  }
  
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, uvals, Verbose=Verbose, speciesList=labels(MySpeciesTree), ...)
  } else {
    pap <- precalcProfs
  }
  l <- length(uvals)
  stopifnot(nrow(pap) > 1)
  
  
  # Initialize Dendrogram in C
  y <- .Call("initCDend", MySpeciesTree, PACKAGE="SynExtend")
  on.exit(rm(y))
  numnodes <- .Call("getTreeNodesCount", y, PACKAGE="SynExtend")
  rn <- rownames(pap)
  glvs <- matrix(NA_integer_, nrow=numnodes, ncol=l)
  allnonzero <- logical(l)
  # Calculate Gain/Loss Vectors
  if (Verbose) cat('  Calculating gain/loss vectors:\n')
  if (Verbose) pb <- txtProgressBar(max=ncol(pap), style=3)
  for (i in seq_len(l)){
    v <- rn[pap[,i]]
    if (length(v) == 0){
      glv <- rep(0L, numnodes)
      allnonzero[i] <- TRUE
    } else {
      glv <- .Call("calcGainLoss", y, v, TRUE, PACKAGE="SynExtend")
    }
    glvs[,i] <- glv
    if (Verbose) setTxtProgressBar(pb, i)
  }
  if(Verbose) cat("\n")
  lenglv <- nrow(glvs)
  ARGS <- list(allnonzero=allnonzero, y=y, bsn=BOOTSTRAP_NUM, l=lenglv)
  FXN <- function(v1, v2, ARGS, ii, jj){
    allnonzero <- ARGS$allnonzero
    if (allnonzero[ii] || allnonzero[jj]){
      return(0)
    } else {
      res <- .Call("calcScoreGL", ARGS$y, v1, v2, PACKAGE="SynExtend")
      if (res==0) return(0)
      num_bs <- ARGS$bsn
      if(num_bs > 0){
        v1 <- as.integer(v1)
        v2 <- as.integer(v2)
        l <- as.integer(ARGS$l)
        replicates <- rep(NA_real_, num_bs)
        for(i in seq_len(num_bs)){
          replicates[i] <- .Call("calcScoreGL", ARGS$y, 
                                     .C("shuffleRInt", v1, l, PACKAGE="SynExtend")[[1]],
                                     .C("shuffleRInt", v2, l, PACKAGE="SynExtend")[[1]], 
                                     PACKAGE="SynExtend")
        }
        normer <- sum(abs(v1), abs(v2))
        normer <- ifelse(normer==0, 1, normer)
        replicates <- 2*replicates / normer
        res <- 2*res / normer
        pv <- sum(replicates <= res) / num_bs
        if(pv > 0.5){
          pv <- 2*(1-pv)
        } else {
          pv <- 2*pv
        }
        res <- (1-pv)*res
      } else{
        normer <- mean(sum(abs(v1)), sum(abs(v2)))
        normer <- sum(abs(v1), abs(v2))
        normer <- ifelse(normer==0, 1, normer)
        res <- 2*res / normer
      }
      return(res)
    }
  }
  pairscores <- BuildSimMatInternal(glvs, uvals, evalmap, l, names(ew), 
                                    FXN, ARGS, Verbose)

  return(pairscores)
}

PAPV.EvoWeaver <- function(ew, Subset=NULL, Verbose=TRUE,
                               precalcProfs=NULL, precalcSubset=NULL, ...){
  
  if (!is.null(precalcSubset))
    subs <- precalcSubset
  else
    subs <- ProcessSubset(ew, Subset)
  uvals <- subs$uvals
  evalmap <- subs$evalmap
  
  if ( is.null(precalcProfs) ){
    if (Verbose) cat('Calculating PA Profiles...\n')
    pap <- PAProfiles(ew, uvals, Verbose=Verbose)
  } else {
    pap <- precalcProfs
  }
  l <- length(uvals)
  n <- names(ew)
  if ( l == 1 ){
    mat <- matrix(1, nrow=1, ncol=1)
    rownames(mat) <- colnames(mat) <- n
    return(mat)
  }
  nr <- nrow(pap)
  pap[] <- as.integer(pap) 
  ARGS <- list(nr=nr)
  FXN <- function(v1, v2, ARGS, ii, jj) {
    if(all(v1==v1[1]) || all(v2==v2[1])) return(0)
    return(1-fisher.test(v1, v2, simulate.p.value=TRUE)$p.value)
  }
  pairscores <- BuildSimMatInternal(pap, uvals, evalmap, l, n, FXN, ARGS, Verbose)
  
  return(pairscores)
}
