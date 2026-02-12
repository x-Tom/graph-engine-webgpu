#pragma once

#include <string>
#include <vector>

struct te_expr;

class ExpressionParser {
public:
	ExpressionParser();
	~ExpressionParser();

	ExpressionParser(const ExpressionParser&) = delete;
	ExpressionParser& operator=(const ExpressionParser&) = delete;

	ExpressionParser(ExpressionParser&& other) noexcept;
	ExpressionParser& operator=(ExpressionParser&& other) noexcept;

	// Compile an expression with named variables.
	// Returns true on success. On failure, errorMsg is set.
	bool compile(const std::string& expr, const std::vector<std::string>& varNames, std::string& errorMsg);

	// Evaluate the compiled expression. values array must match varNames order.
	double evaluate(const double* values);

	bool isValid() const { return m_expr != nullptr; }

	void free();

private:
	te_expr* m_expr = nullptr;
	std::vector<double> m_vars;         // storage bound by pointer to tinyexpr
	std::vector<std::string> m_names;   // keep names alive for te_variable
};
