#include "pa10_declarator_shape.h"

#include <cstddef>

#include "pa10_ast.h"

namespace PA10DeclaratorShape
{
OperatorKind nearest_operator(const PA10AstNode& declarator)
{
	std::size_t direct = declarator.children.size();
	for (std::size_t i = 0; i < declarator.children.size(); ++i)
	{
		const PA10NodeKind kind = declarator.children[i].kind;
		if (kind == PA10NodeKind::Identifier ||
			kind == PA10NodeKind::NestedDeclarator)
		{
			direct = i;
			break;
		}
	}
	if (direct == declarator.children.size())
		return ObjectOperator;

	const PA10AstNode& child = declarator.children[direct];
	if (child.kind == PA10NodeKind::NestedDeclarator &&
		!child.children.empty())
	{
		const OperatorKind nested = nearest_operator(child.children.front());
		if (nested != NoOperator)
			return nested;
	}

	// Parentheses defer; the first suffix or prefix operator outward decides.
	for (std::size_t i = direct + 1; i < declarator.children.size(); ++i)
	{
		const PA10NodeKind kind = declarator.children[i].kind;
		if (kind == PA10NodeKind::ParameterClause)
			return FunctionOperator;
		if (kind == PA10NodeKind::ArraySuffix)
			return ObjectOperator;
	}
	for (std::size_t i = 0; i < direct; ++i)
		if (declarator.children[i].kind == PA10NodeKind::PtrOperator)
			return ObjectOperator;
	return NoOperator;
}

bool is_function(const PA10AstNode& declarator)
{
	return nearest_operator(declarator) == FunctionOperator;
}
}
