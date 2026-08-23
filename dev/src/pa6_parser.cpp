#include "pa6_parser.h"

#include <sstream>

namespace pa6_internal
{

PA6Parser::PA6Parser(const std::vector<PA6Token>& tokens)
	: CppSyntaxCore<PA6Token, PA6SyntaxTraits>(tokens), angle_depth_(0),
	  angle_bases_()
{}

bool PA6Parser::parse(std::string* reason)
{
	if (tokens_.empty() || tokens_.back().kind != PA6TokenKind::ST_EOF)
		return fail(reason, "missing EOF");
	if (!parse_translation_unit() || exhausted_)
	{
		if (exhausted_)
			return fail(reason, "recognizer work bound exceeded");
		std::ostringstream message;
		message << "translation unit does not match PA6 grammar at token "
			<< position_;
		if (position_ < tokens_.size())
		{
			if (tokens_[position_].kind == PA6TokenKind::Fixed)
				message << " (" << simple_token_type_name(tokens_[position_].fixed)
					<< ")";
			else
				message << " (kind " << static_cast<int>(tokens_[position_].kind)
					<< ")";
		}
		if (reason != NULL)
			*reason = message.str();
		return false;
	}
	return true;
}

bool PA6Parser::fail(std::string* reason, const char* message) const
{
	if (reason != NULL)
		*reason = message;
	return false;
}

bool PA6Parser::restore_and_fail(const Mark& saved)
{
	restore(saved);
	return false;
}

bool PA6Parser::tick()
{
	return charge();
}

PA6Parser::Mark PA6Parser::mark() const
{
	Mark result = {position_, angle_depth_, nesting_,
		angle_bases_.size()};
	return result;
}

void PA6Parser::restore(const Mark& saved)
{
	position_ = saved.position;
	angle_depth_ = saved.angle_depth;
	nesting_ = saved.non_angle_depth;
	angle_bases_.resize(saved.angle_base_count);
}

bool PA6Parser::kind(PA6TokenKind wanted, std::size_t offset) const
{
	const PA6Token* token = look(offset);
	return token != NULL && token->kind == wanted;
}

bool PA6Parser::category(unsigned int wanted, std::size_t offset) const
{
	const PA6Token* token = look(offset);
	return token != NULL && token->kind == PA6TokenKind::Identifier &&
		(token->name_categories & wanted) != 0;
}

bool PA6Parser::consume_kind(PA6TokenKind wanted)
{
	if (!kind(wanted) || !tick())
		return false;
	advance();
	return true;
}

bool PA6Parser::consume_fixed(SimpleTokenType wanted)
{
	if (!fixed(wanted) || !tick())
		return false;
	advance();
	return true;
}

bool PA6Parser::consume_identifier()
{
	if (!identifier() || !tick())
		return false;
	advance();
	return true;
}

bool PA6Parser::consume_literal()
{
	if (!literal() || !tick())
		return false;
	advance();
	return true;
}

bool PA6Parser::consume_current()
{
	if (eof() || !tick())
		return false;
	advance();
	return true;
}

bool PA6Parser::begin_non_angle()
{
	return begin_nesting();
}

void PA6Parser::end_non_angle()
{
	end_nesting();
}

bool PA6Parser::begin_angle()
{
	if (angle_depth_ >= PA6SyntaxTraits::max_nesting() || !tick())
	{
		exhausted_ = true;
		return false;
	}
	++angle_depth_;
	angle_bases_.push_back(nesting_);
	return true;
}

bool PA6Parser::close_angle()
{
	if (angle_depth_ == 0 ||
		(!fixed(SimpleTokenType::OP_GT) &&
		 !kind(PA6TokenKind::ST_RSHIFT_1) &&
		 !kind(PA6TokenKind::ST_RSHIFT_2)))
		return false;
	if (fixed(SimpleTokenType::OP_GT))
	{
		if (!consume_fixed(SimpleTokenType::OP_GT))
			return false;
	}
	else if (kind(PA6TokenKind::ST_RSHIFT_1))
	{
		if (!consume_kind(PA6TokenKind::ST_RSHIFT_1))
			return false;
	}
	else if (!consume_kind(PA6TokenKind::ST_RSHIFT_2))
		return false;
	--angle_depth_;
	angle_bases_.pop_back();
	return true;
}

bool PA6Parser::shift_operator()
{
	if (fixed(SimpleTokenType::OP_LSHIFT))
		return consume_fixed(SimpleTokenType::OP_LSHIFT);
	if (!kind(PA6TokenKind::ST_RSHIFT_1) ||
		!kind(PA6TokenKind::ST_RSHIFT_2, 1))
		return false;
	return consume_kind(PA6TokenKind::ST_RSHIFT_1) &&
		consume_kind(PA6TokenKind::ST_RSHIFT_2);
}

bool PA6Parser::can_use_angle_operator() const
{
	return angle_depth_ == 0 ||
		nesting_ > angle_bases_.back();
}

} // namespace pa6_internal
