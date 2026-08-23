#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "posttoken.h"

// Shared production syntax substrate for staged parsers.  Stage policies
// provide only token classification and their assignment-specific budget;
// cursor movement, charging, EOF checks, and nesting bounds have one owner.
template <typename Token, typename Traits>
class CppSyntaxCore
{
protected:
	const std::vector<Token>& tokens_;
	std::size_t position_;
	std::size_t work_;
	std::size_t work_limit_;
	bool exhausted_;
	std::size_t nesting_;

	explicit CppSyntaxCore(const std::vector<Token>& tokens)
		: tokens_(tokens), position_(0), work_(0),
		  work_limit_(Traits::work_limit_for(tokens.size())),
		  exhausted_(false), nesting_(0)
	{}

	bool charge()
	{
		if (work_ >= work_limit_)
		{
			exhausted_ = true;
			return false;
		}
		++work_;
		return true;
	}

	void advance()
	{
		++position_;
	}

	const Token* look(std::size_t offset = 0) const
	{
		if (position_ >= tokens_.size() ||
			offset >= tokens_.size() - position_)
			return NULL;
		return &tokens_[position_ + offset];
	}

	bool eof() const
	{
		const Token* token = look();
		return token == NULL || Traits::is_end(*token);
	}

	bool fixed(SimpleTokenType wanted, std::size_t offset = 0) const
	{
		const Token* token = look(offset);
		return token != NULL && Traits::is_fixed(*token, wanted);
	}

	bool identifier(std::size_t offset = 0) const
	{
		const Token* token = look(offset);
		return token != NULL && Traits::is_identifier(*token);
	}

	bool literal(std::size_t offset = 0) const
	{
		const Token* token = look(offset);
		return token != NULL && Traits::is_literal(*token);
	}

	bool begin_nesting()
	{
		if (nesting_ >= Traits::max_nesting() || !charge())
		{
			exhausted_ = true;
			return false;
		}
		++nesting_;
		return true;
	}

	void end_nesting()
	{
		if (nesting_ != 0)
			--nesting_;
	}
};
