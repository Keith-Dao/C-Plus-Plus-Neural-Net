#pragma once
#include <Eigen/Dense>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <tuple>

namespace activation_functions {
class ActivationFunction;
}
using json = nlohmann::json;

namespace linear {
class Linear {
  std::shared_ptr<Eigen::MatrixXd> input = nullptr;
  Eigen::MatrixXd weight;
  Eigen::VectorXd bias;
  std::shared_ptr<activation_functions::ActivationFunction> activationFunction;
  bool eval = false;

  /*
    Create and set the activation function.
  */
  template <typename T> void setActivation() {
    this->activationFunction = std::make_shared<T>();
  };

public:
  int inChannels, outChannels;
  Linear(int inChannels, int outChannels,
         const std::string &activation = "NoActivation");

#pragma region Properties
#pragma region Evaluation mode
  /*
    Get the layer's evaluation mode.
  */
  bool getEval() const;
  /*
    Set the layer's evaluation mode.
  */
  void setEval(bool eval);
#pragma endregion Evaluation mode

#pragma region Weight
  /*
    Get the layer's weight.
  */
  Eigen::MatrixXd getWeight() const;
  /*
    Set the layer's weight.
  */
  void setWeight(Eigen::MatrixXd weight);
#pragma endregion Weight

#pragma region Bias
  /*
    Get the layer's bias.
  */
  Eigen::VectorXd getBias() const;
  /*
    Set the layer's bias.
  */
  void setBias(Eigen::VectorXd bias);
#pragma endregion Bias

#pragma region Activation function
  /*
    Get the layer's activation function.
  */
  std::shared_ptr<activation_functions::ActivationFunction>
  getActivation() const;
  /*
    Set the layer's activation function.
  */
  void setActivation(std::string activation_function);
#pragma endregion Activation function
#pragma endregion Properties

#pragma region Load
  /*
    Creates a linear instance from the JSON values.
  */
  static Linear fromJson(const json &values);
#pragma endregion Load

#pragma region Save
  /*
    Get all relevant attributes in a serialisable format.

    Attributes includes:
        - weight -- weights as a two-dimensional list
        - bias -- bias as a list
        - activation_function -- name of the activation function as a
                                  string
  */
  json toJson() const;
#pragma endregion Save

#pragma region Forward pass
  /*
    Perform the forward pass for the layer.
  */
  Eigen::MatrixXd forward(const Eigen::MatrixXd &input);
#pragma endregion Forward pass

#pragma region Backward pass
  /*
    Perform the backward pass for the layer.
  */
  std::tuple<Eigen::MatrixXd, Eigen::MatrixXd, Eigen::MatrixXd>
  backward(const Eigen::MatrixXd &grad);
  /*
    Update the parameters of the layer.
  */
  Eigen::MatrixXd update(const Eigen::MatrixXd &grad,
                         const double learningRate);
#pragma endregion Backward pass

#pragma region Builtins
  /*
    Perform the forward pass for the layer.
  */
  Eigen::MatrixXd operator()(const Eigen::MatrixXd &input);

  bool operator==(const Linear &other) const;
#pragma endregion Builtins
};

} // namespace linear
