#include "cross_entropy_loss.hpp"
#include "exceptions.hpp"
#include "utils.hpp"
#include <iostream>

using namespace loss;

#pragma region Reductions
std::unordered_map<std::string, std::function<double(Eigen::MatrixXd)>>
    CrossEntropyLoss::reductions{
        {"mean", [](Eigen::MatrixXd matrix) { return matrix.mean(); }},
        {"sum", [](Eigen::MatrixXd matrix) { return matrix.sum(); }}};
#pragma endregion Reductions

#pragma region Properties
#pragma region Reduction
std::string CrossEntropyLoss::getReduction() const { return this->reduction; }

void CrossEntropyLoss::setReduction(std::string reduction) {
  if (!this->reductions.count(reduction)) {
    throw src_exceptions::InvalidReductionException();
  }
  this->reduction = reduction;
}
#pragma region Reduction
#pragma endregion Properties

#pragma region Forward
double CrossEntropyLoss::forward(const Eigen::MatrixXd &logits,
                                 const Eigen::MatrixXi &targets) {
  if (logits.rows() < 1) {
    throw src_exceptions::EmptyMatrixException("logits");
  }
  if (targets.rows() < 1) {
    throw src_exceptions::EmptyMatrixException("targets");
  }

  if (logits.rows() != targets.rows() || logits.cols() != targets.cols()) {
    throw src_exceptions::InvalidShapeException();
  }
  this->targets = std::make_shared<Eigen::MatrixXi>(targets);
  this->probabilities =
      std::make_shared<Eigen::MatrixXd>(utils::softmax(logits));
  return CrossEntropyLoss::reductions[this->reduction](
      -(targets.cast<double>().cwiseProduct(utils::log_softmax(logits)))
           .rowwise()
           .sum());
}

double CrossEntropyLoss::forward(const Eigen::MatrixXd &logits,
                                 const std::vector<int> &targets) {
  return this->forward(logits, utils::one_hot_encode(targets, logits.cols()));
}
#pragma endregion Forward