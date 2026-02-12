#include "ExpressionParser.h"
#include "tinyexpr/tinyexpr.h"
#include <cmath>

ExpressionParser::ExpressionParser() {}

ExpressionParser::~ExpressionParser() {
	free();
}

ExpressionParser::ExpressionParser(ExpressionParser&& other) noexcept
	: m_expr(other.m_expr), m_vars(std::move(other.m_vars)), m_names(std::move(other.m_names)) {
	other.m_expr = nullptr;
}

ExpressionParser& ExpressionParser::operator=(ExpressionParser&& other) noexcept {
	if (this != &other) {
		free();
		m_expr = other.m_expr;
		m_vars = std::move(other.m_vars);
		m_names = std::move(other.m_names);
		other.m_expr = nullptr;
	}
	return *this;
}

void ExpressionParser::free() {
	if (m_expr) {
		te_free(m_expr);
		m_expr = nullptr;
	}
}

bool ExpressionParser::compile(const std::string& expr, const std::vector<std::string>& varNames, std::string& errorMsg) {
	free();

	// Pre-allocate variable storage (must not reallocate after te_compile)
	m_names = varNames;
	m_vars.resize(varNames.size(), 0.0);

	// Build te_variable array
	std::vector<te_variable> vars(varNames.size());
	for (size_t i = 0; i < varNames.size(); ++i) {
		vars[i].name = m_names[i].c_str();
		vars[i].address = &m_vars[i];
		vars[i].type = TE_VARIABLE;
		vars[i].context = nullptr;
	}

	int error = 0;
	m_expr = te_compile(expr.c_str(), vars.data(), static_cast<int>(vars.size()), &error);

	if (!m_expr) {
		errorMsg = "Parse error at position " + std::to_string(error);
		return false;
	}

	errorMsg.clear();
	return true;
}

double ExpressionParser::evaluate(const double* values) {
	if (!m_expr) return 0.0;

	// Copy values into bound storage
	for (size_t i = 0; i < m_vars.size(); ++i) {
		m_vars[i] = values[i];
	}

	return te_eval(m_expr);
}
