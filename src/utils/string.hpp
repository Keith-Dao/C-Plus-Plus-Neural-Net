#pragma once
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <vector>

namespace utils::string {
/*
  Splits the given string by the given delimiter.
*/
std::vector<std::string> split(const std::string &s,
                               const std::string &delimiter);

/*
  Joins the given strings by the given token.
*/
std::string join(const std::vector<std::string> &words,
                 const std::string &joiner);

/*
  Convert a float to a set precision string.
*/
std::string floatToString(float num, int precision);

/*
  Capitalise the first letter of the given word.
*/
std::string capitalise(std::string word);

/*
  Join a list of items but use a different connector for the last pair
*/
std::string joinWithDifferentLast(std::vector<std::string> words,
                                  const std::string &connector,
                                  const std::string &lastConnector);

/*
  Trim spaces from the left.
*/
void ltrim(std::string &word);

/*
  Trim spaces from the right.
*/
void rtrim(std::string &word);

/*
  Trim spaces on both sides.
*/
void trim(std::string &word);
} // namespace utils::string