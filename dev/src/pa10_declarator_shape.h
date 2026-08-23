#pragma once

struct PA10AstNode;

namespace PA10DeclaratorShape
{
enum OperatorKind
{
	NoOperator,
	FunctionOperator,
	ObjectOperator
};

OperatorKind nearest_operator(const PA10AstNode& declarator);
bool is_function(const PA10AstNode& declarator);
}
