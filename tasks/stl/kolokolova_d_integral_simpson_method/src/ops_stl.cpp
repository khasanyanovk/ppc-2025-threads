#include "stl/kolokolova_d_integral_simpson_method/include/ops_stl.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

#include "core/util/include/util.hpp"

bool kolokolova_d_integral_simpson_method_stl::TestTaskSTL::PreProcessingImpl() {
  nums_variables_ = int(task_data->inputs_count[0]);

  steps_ = std::vector<int>(task_data->inputs_count[0]);
  auto* input_steps = reinterpret_cast<int*>(task_data->inputs[0]);
  for (unsigned i = 0; i < task_data->inputs_count[0]; i++) {
    steps_[i] = input_steps[i];
  }

  borders_ = std::vector<int>(task_data->inputs_count[1]);
  auto* input_borders = reinterpret_cast<int*>(task_data->inputs[1]);
  for (unsigned i = 0; i < task_data->inputs_count[1]; i++) {
    borders_[i] = input_borders[i];
  }

  result_output_ = 0;
  return true;
}

bool kolokolova_d_integral_simpson_method_stl::TestTaskSTL::ValidationImpl() {
  // Check inputs and outputs
  std::vector<int> bord = std::vector<int>(task_data->inputs_count[1]);
  auto* input_bord = reinterpret_cast<int*>(task_data->inputs[1]);
  for (unsigned i = 0; i < task_data->inputs_count[1]; i++) {
    bord[i] = input_bord[i];
  }
  int num_var = int(task_data->inputs_count[0]);
  int num_bord = int(task_data->inputs_count[1]) / 2;
  return (task_data->inputs_count[0] != 0 && task_data->inputs_count[1] != 0 && task_data->outputs_count[0] != 0 &&
          CheckBorders(bord) && num_var == num_bord);
  return true;
}

bool kolokolova_d_integral_simpson_method_stl::TestTaskSTL::RunImpl() {
  std::vector<double> size_step(nums_variables_);

  auto calculate_size_step = [&](int i) {
    double a = double(borders_[(2 * i) + 1] - borders_[2 * i]) / double(steps_[i]);
    size_step[i] = a;
  };

  std::vector<std::thread> threads;
  threads.reserve(nums_variables_);
  for (int i = 0; i < nums_variables_; ++i) {
    threads.emplace_back(calculate_size_step, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  std::vector<std::vector<double>> points(nums_variables_);
  threads.clear();

  for (int i = 0; i < nums_variables_; ++i) {
    threads.emplace_back([&, i]() {
      std::vector<double> vec;
      for (int j = 0; j <= steps_[i]; ++j) {
        double num = borders_[2 * i] + (double(j) * size_step[i]);
        vec.push_back(num);
      }
      points[i] = vec;
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  std::vector<double> results_func = FindFunctionValue(points, func_);
  std::vector<double> coeff = FindCoeff(steps_[0]);
  MultiplyCoeffandFunctionValue(results_func, coeff, nums_variables_);
  result_output_ = CreateOutputResult(results_func, size_step);

  return true;
}

bool kolokolova_d_integral_simpson_method_stl::TestTaskSTL::PostProcessingImpl() {
  reinterpret_cast<double*>(task_data->outputs[0])[0] = result_output_;
  return true;
}

std::vector<double> kolokolova_d_integral_simpson_method_stl::TestTaskSTL::FindFunctionValue(
    const std::vector<std::vector<double>>& coordinates, const std::function<double(std::vector<double>)>& f) {
  std::vector<double> results;                                     // result of function
  std::vector<double> current;                                     // current point
  GeneratePointsAndEvaluate(coordinates, 0, current, results, f);  // recursive function
  return results;
}

void kolokolova_d_integral_simpson_method_stl::TestTaskSTL::GeneratePointsAndEvaluate(
    const std::vector<std::vector<double>>& coordinates, int index, std::vector<double>& current,
    std::vector<double>& results, const std::function<double(const std::vector<double>)>& f) {
  // if it the end of vector
  if (index == int(coordinates.size())) {
    double result = f(current);  // find value of function
    results.push_back(result);   // save result
    return;
  }

  // sort through the coordinates
  for (double coord : coordinates[index]) {
    current.push_back(coord);
    GeneratePointsAndEvaluate(coordinates, index + 1, current, results, func_);  // recursive
    current.pop_back();                                                          // delete for next coordinat
  }
}

std::vector<double> kolokolova_d_integral_simpson_method_stl::TestTaskSTL::FindCoeff(int count_step) {
  std::vector<double> result_coeff(1, 1.0);  // first coeff is always 1
  for (int i = 1; i < count_step; i++) {
    if (i % 2 != 0) {
      result_coeff.push_back(4.0);  // odd coeff is 4
    } else {
      result_coeff.push_back(2.0);  // even coeff is 2
    }
  }
  result_coeff.push_back(1.0);  // last coeff is always 1
  return result_coeff;
}

void kolokolova_d_integral_simpson_method_stl::TestTaskSTL::MultiplyCoeffandFunctionValue(
    std::vector<double>& function_val, const std::vector<double>& coeff_vec, int a) {
  int coeff_vec_size = int(coeff_vec.size());
  int function_vec_size = int(function_val.size());

  // Determine the number of threads for parallel processing using GetPPCNumThreads
  int thread_count = ppc::util::GetPPCNumThreads();
  if (thread_count <= 0 || thread_count > int(std::thread::hardware_concurrency())) {
    // Use maximum available threads if GetPPCNumThreads gives a non-viable count
    thread_count = int(std::thread::hardware_concurrency());
  }

  // vector for storing threads
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  // calculate the section size for each thread
  int section_size = function_vec_size / thread_count;

  // loop of multiplying function values ??by coefficients
  for (int t = 0; t < thread_count; ++t) {
    threads.emplace_back([&, t, section_size, function_vec_size, coeff_vec_size]() {
      int start = t * section_size;

      // determine the start and end indices for the current thread
      int end = (t == thread_count - 1) ? function_vec_size : start + section_size;
      for (int i = start; i < end; ++i) {
        function_val[i] *= coeff_vec[i % coeff_vec_size];
      }
    });
  }

  // wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }

  // additional iterations of multiplication by coefficients
  for (int iteration = 1; iteration < a; ++iteration) {
    threads.clear();
    for (int t = 0; t < thread_count; ++t) {
      threads.emplace_back([&, t, section_size, function_vec_size, coeff_vec_size, iteration]() {
        int start = t * section_size;

        // determine the start and end indices for the current thread
        int end = (t == thread_count - 1) ? function_vec_size : start + section_size;
        for (int i = start; i < end; ++i) {
          int block_size = iteration * coeff_vec_size;
          int current_n_index = (i / block_size) % coeff_vec_size;
          function_val[i] *= coeff_vec[current_n_index];
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }
  }
}

double kolokolova_d_integral_simpson_method_stl::TestTaskSTL::CreateOutputResult(std::vector<double> const& vec,
                                                                                 std::vector<double> size_steps) const {
  double sum = 0;

  std::vector<std::thread> threads;
  int thread_count = int(std::thread::hardware_concurrency());
  if (thread_count == 0) {
    thread_count = 1;
  }
  threads.reserve(thread_count);
  int section_size = int(vec.size()) / thread_count;
  std::vector<double> local_sums(thread_count, 0.0);

  for (int t = 0; t < thread_count; ++t) {
    threads.emplace_back([&, t, section_size, vec]() {
      int start = t * section_size;
      int end = (t == thread_count - 1) ? int(vec.size()) : start + section_size;
      double local_sum = 0;
      for (int i = start; i < end; i++) {
        local_sum += vec[i];
      }
      local_sums[t] = local_sum;
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  for (const auto& local_sum : local_sums) {
    sum += local_sum;
  }

  for (size_t i = 0; i < size_steps.size(); i++) {
    sum *= size_steps[i];
  }

  sum /= pow(3, nums_variables_);

  return sum;
}

bool kolokolova_d_integral_simpson_method_stl::TestTaskSTL::CheckBorders(std::vector<int> vec) {
  size_t i = 0;
  while (i < vec.size()) {
    if (vec[i] > vec[i + 1]) {
      return false;
    }
    i += 2;
  }
  return true;
}
