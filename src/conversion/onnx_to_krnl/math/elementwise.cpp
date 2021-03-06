//===----- elementwise.cpp - Elementwise Ops ------------------------------===//
//
// Copyright 2019 The IBM Research Authors.
//
// =============================================================================
//
// This file lowers ONNX element-wise operators to Krnl dialect.
//
//===----------------------------------------------------------------------===//

#include "src/conversion/onnx_to_krnl/onnx_to_krnl_common.hpp"

using namespace mlir;

template <>
struct ScalarOp<ONNXAddOp> {
  using FOp = AddFOp;
  using IOp = AddIOp;
};

template <>
struct ScalarOp<ONNXMulOp> {
  using FOp = MulFOp;
  using IOp = MulIOp;
};

template <>
struct ScalarOp<ONNXDivOp> {
  using FOp = DivFOp;
  using IOp = SignedDivIOp;
};

template <>
struct ScalarOp<ONNXSubOp> {
  using FOp = SubFOp;
  using IOp = SubIOp;
};

template <>
struct ScalarOp<ONNXAndOp> {
  using FOp = AndOp; // not use
  using IOp = AndOp;
};

template <>
struct ScalarOp<ONNXOrOp> {
  using FOp = OrOp; // not use
  using IOp = OrOp;
};

template <>
struct ScalarOp<ONNXXorOp> {
  using FOp = XOrOp; // not use
  using IOp = XOrOp;
};

template <>
struct ScalarOp<ONNXExpOp> {
  using FOp = ExpOp;
  using IOp = ExpOp; // not use
};

template <>
struct ScalarOp<ONNXSumOp> {
  using FOp = AddFOp;
  using IOp = AddIOp;
};

template <>
struct ScalarOp<ONNXTanhOp> {
  using FOp = TanhOp;
  using IOp = TanhOp; // not use
};

template <>
struct ScalarOp<ONNXCosOp> {
  using FOp = CosOp;
  using IOp = CosOp; // not use
};

template <>
struct ScalarOp<ONNXLogOp> {
  using FOp = LogOp;
  using IOp = LogOp; // not use
};

template <>
struct ScalarOp<ONNXSqrtOp> {
  using FOp = SqrtOp;
  using IOp = SqrtOp; // not use
};

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXSinhOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXSinhOp>(Operation *op, ArrayRef<Type> result_types,
                                     ArrayRef<Value> operands,
                                     ConversionPatternRewriter &rewriter) {
  // ONNXSinhOp(%X) = DivFOp(SubFOp(ExpOp(%X), ExpOp(NegFOp(%X))),
  //                         ConstantOp 2)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto two =  emitConstantOp(rewriter, loc, elementType, 2);
  auto neg = rewriter.create<SubFOp>(loc, zero, operand);
  auto exp = rewriter.create<ExpOp>(loc, operand);
  auto negExp = rewriter.create<ExpOp>(loc, neg);
  auto result = rewriter.create<DivFOp>(
      loc, rewriter.create<SubFOp>(loc, exp, negExp), two);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXCoshOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXCoshOp>(Operation *op, ArrayRef<Type> result_types,
                                     ArrayRef<Value> operands,
                                     ConversionPatternRewriter &rewriter) {
  // ONNXCoshOp(%X) = DivFOp(AddFOp(ExpOp(%X), ExpOp(NegFOp(%X))),
  //                         ConstantOp 2)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto two =  emitConstantOp(rewriter, loc, elementType, 2);
  auto neg = rewriter.create<SubFOp>(loc, zero, operand);
  auto exp = rewriter.create<ExpOp>(loc, operand);
  auto negExp = rewriter.create<ExpOp>(loc, neg);
  auto result = rewriter.create<DivFOp>(
      loc, rewriter.create<AddFOp>(loc, exp, negExp), two);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXSigmoidOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXSigmoidOp>(Operation *op,
                                        ArrayRef<Type> result_types,
                                        ArrayRef<Value> operands,
                                        ConversionPatternRewriter &rewriter) {
  // ONNXSigmoidOp(%X) = DivFOp(ConstantOp 1,
  //                            AddFOp(ConstantOp 1, ExpOp(NegFOp(%X))))
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto one = emitConstantOp(rewriter, loc, elementType, 1);
  auto neg = rewriter.create<SubFOp>(loc, zero, operand);
  auto negExp = rewriter.create<ExpOp>(loc, neg);
  auto result = rewriter.create<DivFOp>(
      loc, one, rewriter.create<AddFOp>(loc, one, negExp));

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXHardSigmoidOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXHardSigmoidOp>(
    Operation *op, ArrayRef<Type> result_types, ArrayRef<Value> operands,
    ConversionPatternRewriter &rewriter) {
  // %Y = AddFOp(MulFOp(alpha, %X), beta)
  // %Z = SelectOp(CmpFOp(OGT, %Y, Constant 0),
  //               %Y,
  //               Constant 0)
  // ONNXHardSigmoidOp(%X) = SelectOp(CmpFOp(OLT, %Z, Constant 1),
  //                                  %Z,
  //                                  Constant 1)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto alphaAttribute = FloatAttr::get(rewriter.getF32Type(),
      llvm::dyn_cast<ONNXHardSigmoidOp>(op).alpha().convertToFloat());
  auto betaAttribute = FloatAttr::get(rewriter.getF32Type(),
      llvm::dyn_cast<ONNXHardSigmoidOp>(op).beta().convertToFloat());
  auto elementType = result_types[0];

  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto one = emitConstantOp(rewriter, loc, elementType, 1);
  auto alpha = rewriter.create<ConstantOp>(loc, alphaAttribute);
  auto beta = rewriter.create<ConstantOp>(loc, betaAttribute);

  auto add = rewriter.create<AddFOp>(
      loc, rewriter.create<MulFOp>(loc, alpha, operand), beta);
  auto maxPredicate =
      rewriter.create<CmpFOp>(loc, CmpFPredicate::OGT, add, zero);
  auto max = rewriter.create<SelectOp>(loc, maxPredicate, add, zero);
  auto minPredicate =
      rewriter.create<CmpFOp>(loc, CmpFPredicate::OLT, max, one);
  auto result = rewriter.create<SelectOp>(loc, minPredicate, max, one);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXEluOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXEluOp>(Operation *op, ArrayRef<Type> result_types,
                                    ArrayRef<Value> operands,
                                    ConversionPatternRewriter &rewriter) {
  // ONNXEluOp(%X) = SelectOp(CmpFOp(OLT, %X, ConstantOp 0),
  //                          MulFOp(alpha, SubFOp(ExpOp(%X), 1)),
  //                          %X)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto alphaAttribute = FloatAttr::get(rewriter.getF32Type(),
      llvm::dyn_cast<ONNXEluOp>(op).alpha().convertToFloat());
  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto one = emitConstantOp(rewriter, loc, elementType, 1);
  auto alpha = rewriter.create<ConstantOp>(loc, alphaAttribute);
  auto exp = rewriter.create<ExpOp>(loc, operand);
  auto lessThanZero =
      rewriter.create<CmpFOp>(loc, CmpFPredicate::OLT, operand, zero);
  auto result = rewriter.create<SelectOp>(
      loc, lessThanZero,
      rewriter.create<MulFOp>(loc, alpha,
                              rewriter.create<SubFOp>(loc, exp, one)),
      operand);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXReluOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXReluOp>(Operation *op, ArrayRef<Type> result_types,
                                     ArrayRef<Value> operands,
                                     ConversionPatternRewriter &rewriter) {
  // ONNXReluOp(%X) = SelectOp(CmpFOp(OLT, %X, ConstantOp 0),
  //                           ConstantOp 0,
  //                           %X)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto lessThanZero =
      rewriter.create<CmpFOp>(loc, CmpFPredicate::OLT, operand, zero);
  auto result = rewriter.create<SelectOp>(loc, lessThanZero, zero, operand);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXLeakyReluOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXLeakyReluOp>(Operation *op,
                                          ArrayRef<Type> result_types,
                                          ArrayRef<Value> operands,
                                          ConversionPatternRewriter &rewriter) {
  // ONNXLeakyReluOp(%X) = SelectOp(CmpFOp(OLT, %X, ConstantOp 0),
  //                                MulFOp(alpha, %X),
  //                                %X)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto alphaAttribute = FloatAttr::get(rewriter.getF32Type(),
      llvm::dyn_cast<ONNXLeakyReluOp>(op).alpha().convertToFloat());
  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto alpha = rewriter.create<ConstantOp>(loc, alphaAttribute);
  auto lessThanZero =
      rewriter.create<CmpFOp>(loc, CmpFPredicate::OLT, operand, zero);
  auto result = rewriter.create<SelectOp>(
      loc, lessThanZero, rewriter.create<MulFOp>(loc, alpha, operand), operand);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXSeluOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXSeluOp>(Operation *op, ArrayRef<Type> result_types,
                                     ArrayRef<Value> operands,
                                     ConversionPatternRewriter &rewriter) {
  // ONNXSeluOp(%X) = SelectOp(CmpFOp(OGT, %X, ConstantOp 0),
  //                           MulFOp(gamma, %X),
  //                           MulFOp(gamma,
  //                                  SubFOp(MulFOp(alpha, ExpOp(%X)),
  //                                         alpha)))
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto alphaAttribute = FloatAttr::get(rewriter.getF32Type(),
      llvm::dyn_cast<ONNXSeluOp>(op).alpha().convertToFloat());
  auto gammaAttribute = FloatAttr::get(rewriter.getF32Type(),
      llvm::dyn_cast<ONNXSeluOp>(op).gamma().convertToFloat());
  auto elementType = result_types[0];

  auto zero = emitConstantOp(rewriter, loc, elementType, 0);
  auto alpha = rewriter.create<ConstantOp>(loc, alphaAttribute);
  auto gamma = rewriter.create<ConstantOp>(loc, gammaAttribute);
  auto exp = rewriter.create<ExpOp>(loc, operand);
  auto greaterThanZero =
      rewriter.create<CmpFOp>(loc, CmpFPredicate::OGT, operand, zero);
  auto select = rewriter.create<SelectOp>(
      loc, greaterThanZero, operand,
      rewriter.create<SubFOp>(loc, rewriter.create<MulFOp>(loc, alpha, exp),
                              alpha));
  auto result = rewriter.create<MulFOp>(loc, gamma, select);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXReciprocalOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXReciprocalOp>(
    Operation *op, ArrayRef<Type> result_types, ArrayRef<Value> operands,
    ConversionPatternRewriter &rewriter) {
  // ONNXReciprocalOp(%X) = DivFOp(ConstantOp 1, %X)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto one = emitConstantOp(rewriter, loc, elementType, 1);
  auto result = rewriter.create<DivFOp>(loc, one, operand);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXSoftplusOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXSoftplusOp>(
    Operation *op, ArrayRef<Type> result_types, ArrayRef<Value> operands,
    ConversionPatternRewriter &rewriter) {
  // ONNXSoftplusOp(%X) = LogOp(AddFOp(ExpOp(%X), ConstantOp 1))
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto exp = rewriter.create<ExpOp>(loc, operand);
  auto one = emitConstantOp(rewriter, loc, elementType, 1);
  auto add = rewriter.create<AddFOp>(loc, exp, one);
  auto result = rewriter.create<LogOp>(loc, add);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXSoftsignOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXSoftsignOp>(
    Operation *op, ArrayRef<Type> result_types, ArrayRef<Value> operands,
    ConversionPatternRewriter &rewriter) {
  // ONNXSoftsignOp(%X) = DivFOp(ConstantOp 1, %X)
  auto loc = op->getLoc();
  Value operand = operands[0];
  auto elementType = result_types[0];

  auto abs = rewriter.create<AbsFOp>(loc, operand);
  auto one = emitConstantOp(rewriter, loc, elementType, 1);
  auto add = rewriter.create<AddFOp>(loc, abs, one);
  auto result = rewriter.create<DivFOp>(loc, operand, add);

  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXSignOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXSignOp>(Operation *op, ArrayRef<Type> result_types,
                                     ArrayRef<Value> operands,
                                     ConversionPatternRewriter &rewriter) {

  auto loc = op->getLoc();
  Value operand = operands[0];
  Type elementType = operands.front().getType();
  // TODO: unsigned int should be supported separately?
  if (elementType.isa<IntegerType>()) {
    // %Y = SelectOP(CmpIOp(GT, %X, ConstantOp 0),
    //               ConstantOp 1,
    //               COnstantOp -1)
    // ONNXSignOp(%X) = SelectOP(CmpIOp(EQ, %X, ConstantOp 0),
    //                           ConstantOp 0,
    //                           %Y)
    auto zero = emitConstantOp(rewriter, loc, elementType, 0);
    auto one = emitConstantOp(rewriter, loc, elementType, 1);
    auto minusOne = emitConstantOp(rewriter, loc, elementType, -1);
    auto plusPredicate =
        rewriter.create<CmpIOp>(loc, CmpIPredicate::sgt, operand, zero);
    auto plusSelect =
        rewriter.create<SelectOp>(loc, plusPredicate, one, minusOne);
    auto zeroPredicate =
        rewriter.create<CmpIOp>(loc, CmpIPredicate::eq, operand, zero);
    auto result =
        rewriter.create<SelectOp>(loc, zeroPredicate, zero, plusSelect);
    return result;
  } else if (elementType.isa<FloatType>()) {
    // %Y = SelectOP(CmpFOp(OGT, %X, ConstantOp 0),
    //               ConstantOp 1,
    //               ConstantOp -1)
    // ONNXSignOp(%X) = SelectOP(CmpFOp(OEQ, %X, ConstantOp 0),
    //                           ConstantOp 0,
    //                           %Y)
    auto zero = emitConstantOp(rewriter, loc, elementType, 0);
    auto one = emitConstantOp(rewriter, loc, elementType, 1);
    auto minusOne = emitConstantOp(rewriter, loc, elementType, -1);
    auto plusPredicate =
        rewriter.create<CmpFOp>(loc, CmpFPredicate::OGT, operand, zero);
    auto plusSelect =
        rewriter.create<SelectOp>(loc, plusPredicate, one, minusOne);
    auto zeroPredicate =
        rewriter.create<CmpFOp>(loc, CmpFPredicate::OEQ, operand, zero);
    auto result =
        rewriter.create<SelectOp>(loc, zeroPredicate, zero, plusSelect);
    return result;
  } else {
    emitError(loc, "unsupported element type");
  }
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXMaxOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXMaxOp>(Operation *op, ArrayRef<Type> result_types,
                                    ArrayRef<Value> operands,
                                    ConversionPatternRewriter &rewriter) {
  // ONNXMaxOp(%X, %Y) = SelectOp(CmpFOp(OGT, %X, %Y),
  //                              %X,
  //                              %Y)
  auto loc = op->getLoc();
  Value lhs = operands[0];
  Value rhs = operands[1];
  auto max = rewriter.create<CmpFOp>(loc, CmpFPredicate::OGT, lhs, rhs);
  auto result = rewriter.create<SelectOp>(loc, max, lhs, rhs);
  return result;
}

//===----------------------------------------------------------------------===//
// Scalar unary ops for lowering ONNXMinOp
//===----------------------------------------------------------------------===//
template <>
Value mapToLowerScalarOp<ONNXMinOp>(Operation *op, ArrayRef<Type> result_types,
                                    ArrayRef<Value> operands,
                                    ConversionPatternRewriter &rewriter) {
  // ONNXMinOp(%X, %Y) = SelectOp(CmpFOp(OLT, %X, %Y),
  //                              %X,
  //                              %Y)
  auto loc = op->getLoc();
  Value lhs = operands[0];
  Value rhs = operands[1];
  auto min = rewriter.create<CmpFOp>(loc, CmpFPredicate::OLT, lhs, rhs);
  auto result = rewriter.create<SelectOp>(loc, min, lhs, rhs);
  return result;
}

// Element-wise unary ops lowering to Krnl dialect.
//===----------------------------------------------------------------------===//
template <typename ElementwiseUnaryOp>
struct ONNXElementwiseUnaryOpLowering : public ConversionPattern {
  ONNXElementwiseUnaryOpLowering(MLIRContext *ctx)
      : ConversionPattern(ElementwiseUnaryOp::getOperationName(), 1, ctx) {}
  PatternMatchResult
  matchAndRewrite(Operation *op, ArrayRef<Value> operands,
                  ConversionPatternRewriter &rewriter) const final {
    // TODO: Check that the types are valid.
    // An element-wise unary operation must have all operands and the result of
    // the same type. This should have been verified by the verifier.
    auto loc = op->getLoc();

    // Insert an allocation and deallocation for the result of this operation.
    auto memRefType = convertToMemRefType(*op->result_type_begin());

    // If the output has a dynamic dimension, pass the operands required for
    // each dynamic dimension to the AllocOp. The first operand of the
    // operation is used. The operands of the op need to match in terms of
    // dimensions with the result at this pre-optimization phase.
    // TODO: verify that dimensions match.
    // TODO: can the dimension of the result differ after optimizations?
    Value alloc;
    bool insertDealloc = checkInsertDealloc(op);

    if (hasAllConstantDimensions(memRefType))
      alloc = insertAllocAndDealloc(memRefType, loc, rewriter, insertDealloc);
    else
      alloc = insertAllocAndDealloc(memRefType, loc, rewriter, insertDealloc,
                                    {operands[0]});

    std::vector<Value> originalLoops;
    KrnlOptimizeLoopsOp optimizedLoopsOp;
    KrnlIterateOp iterateOp;
    emitKrnlLoopsAndIterationForOperand(
        rewriter, loc, operands[0], originalLoops,
        optimizedLoopsOp, iterateOp);
    Block &optimizationBlock = optimizedLoopsOp.region().front();
    Block &iterationBlock = iterateOp.bodyRegion().front();

    // 1. Insert any optimizations in the KrnlOptimizeLoopsOp body.
    rewriter.setInsertionPointToEnd(&optimizationBlock);
    // Return from KrnlOptimizeLoopsOp body.
    // When no optimizations are present we just return the loops
    // unchaged.
    rewriter.create<KrnlReturnLoopsOp>(loc, originalLoops);

    // 2. Insert instructions inside the KernelIterateOp body.
    rewriter.setInsertionPointToStart(&iterationBlock);

    // Handle the operation:
    SmallVector<Value, 4> loopIVs;
    for (auto arg : iterationBlock.getArguments())
      loopIVs.push_back(arg);

    auto loadedVal = rewriter.create<LoadOp>(loc, operands[0], loopIVs);
    auto loweredOpResult = mapToLowerScalarOp<ElementwiseUnaryOp>(
        op, memRefType.getElementType(), {loadedVal}, rewriter);
    // Store result in the resulting array.
    rewriter.create<StoreOp>(loc, loweredOpResult, alloc, loopIVs);

    rewriter.replaceOp(op, alloc);

    return matchSuccess();
  }
};

// Element-wise variadic ops lowering to Krnl dialect.
//===----------------------------------------------------------------------===//
template <typename ElementwiseVariadicOp>
struct ONNXElementwiseVariadicOpLowering : public ConversionPattern {
  ONNXElementwiseVariadicOpLowering(MLIRContext *ctx)
      : ConversionPattern(ElementwiseVariadicOp::getOperationName(), 1, ctx) {}
  PatternMatchResult
  matchAndRewrite(Operation *op, ArrayRef<Value> operands,
                  ConversionPatternRewriter &rewriter) const final {
    // TODO: Check that the types are valid.
    // An element-wise variadic operation must have all operands and the result
    // of the same type. This should have been verified by the verifier.
    auto loc = op->getLoc();
    auto numArgs = op->getNumOperands();

    // Insert an allocation and deallocation for the result of this operation.
    auto memRefType = convertToMemRefType(*op->result_type_begin());

    Value alloc;
    bool insertDealloc = checkInsertDealloc(op);
    // If the output has a dynamic dimension, we compute its dimension at
    // runtime by using dimensions from the operands.
    // In particular, we need to know from which operand a result dimension
    // comes from.
    // TODO: can the dimension of the result differ after optimizations?
    if (hasAllConstantDimensions(memRefType))
      alloc = insertAllocAndDealloc(memRefType, loc, rewriter, insertDealloc);
    else
      alloc = insertAllocAndDealloc(memRefType, loc, rewriter, insertDealloc,
                                    operands);

    // Get run-time dimension information for unknown dimensions used for
    // broadcasting.
    std::map<int, std::map<int, Value>> broadcastedDimInfo =
        getBroadcastedDimInfo(loc, rewriter, memRefType, operands);

    std::vector<Value> originalLoops;
    KrnlOptimizeLoopsOp optimizedLoopsOp;
    KrnlIterateOp iterateOp;
    emitKrnlLoopsAndIterationForOperand(
        rewriter, loc, alloc, originalLoops,
        optimizedLoopsOp, iterateOp);
    Block &optimizationBlock = optimizedLoopsOp.region().front();
    Block &iterationBlock = iterateOp.bodyRegion().front();

    // 1. Insert any optimizations in the KrnlOptimizeLoopsOp body.
    rewriter.setInsertionPointToEnd(&optimizationBlock);
    // Return from KrnlOptimizeLoopsOp body.
    // When no optimizations are present we just return the loops unchaged.
    rewriter.create<KrnlReturnLoopsOp>(loc, originalLoops);

    // 2. Insert instructions inside the KernelIterateOp body.
    rewriter.setInsertionPointToStart(&iterationBlock);

    // Handle the operation:
    SmallVector<Value, 4> loopIVs;
    for (auto arg : iterationBlock.getArguments())
      loopIVs.push_back(arg);

    // Fold over operands for each of their scalar values
    Value accumulated, next;
    auto accumulatedLoopIVs = getLoopIVsForBroadcasting(
        loc, rewriter, loopIVs, operands[0], broadcastedDimInfo[0]);
    accumulated = rewriter.create<LoadOp>(loc, operands[0], accumulatedLoopIVs);
    for (unsigned i = 1; i < numArgs; i++) {
      auto nextLoopIVs = getLoopIVsForBroadcasting(
          loc, rewriter, loopIVs, operands[i], broadcastedDimInfo[i]);
      next = rewriter.create<LoadOp>(loc, operands[i], nextLoopIVs);
      accumulated = mapToLowerScalarOp<ElementwiseVariadicOp>(
          op, memRefType.getElementType(), {accumulated, next}, rewriter);
    }
    // Store result in the resulting array.
    rewriter.create<StoreOp>(loc, accumulated, alloc, loopIVs);

    rewriter.replaceOp(op, alloc);

    return matchSuccess();
  }
};

void populateLoweringONNXElementwiseOpPattern(
    OwningRewritePatternList &patterns, MLIRContext *ctx) {
  patterns.insert<ONNXElementwiseVariadicOpLowering<mlir::ONNXAddOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXAndOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXCosOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXCoshOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXDivOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXEluOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXExpOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXHardSigmoidOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXLeakyReluOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXLogOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXMaxOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXMinOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXMulOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXOrOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXReciprocalOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXReluOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXSeluOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXSigmoidOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXSignOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXSinhOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXSoftplusOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXSoftsignOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXSqrtOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXSubOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXSumOp>,
                  ONNXElementwiseUnaryOpLowering<mlir::ONNXTanhOp>,
                  ONNXElementwiseVariadicOpLowering<mlir::ONNXXorOp>>(ctx);
}
