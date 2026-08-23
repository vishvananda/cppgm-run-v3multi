#include "pa10_ast.h"

#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace
{

class PA10PostTokenCollector : public IPostTokenOutput
{
public:
	PA10PostTokenCollector() : tokens(), invalid(false) {}

	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid = true;
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		if (type == SimpleTokenType::OP_RSHIFT)
		{
			// Keep the producer's single shift token as two logical `>` pieces.
			// The parser consumes them as a shift outside an angle context and
			// as one or two close angles inside nested template-ids.
			tokens.push_back(PA10Token(PA10TokenKind::RShiftPiece1,
				SimpleTokenType::OP_GT, 0, ">"));
			tokens.push_back(PA10Token(PA10TokenKind::RShiftPiece2,
				SimpleTokenType::OP_GT, 0, ">"));
		}
		else
			tokens.push_back(PA10Token(PA10TokenKind::Fixed, type, 0, source));
	}

	void emit_identifier(const std::string& source)
	{
		emit_identifier_with_spelling(0, source);
	}

	void emit_simple_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source, SimpleTokenType type)
	{
		tokens.push_back(PA10Token(PA10TokenKind::Fixed, type, spelling, source));
	}

	void emit_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source)
	{
		tokens.push_back(PA10Token(PA10TokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, spelling, source));
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		PA10Token token(PA10TokenKind::Literal, SimpleTokenType::OP_SEMICOLON,
			0, source);
		token.literal = value;
		tokens.push_back(token);
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		PA10Token token(PA10TokenKind::UserDefinedLiteral,
			SimpleTokenType::OP_SEMICOLON, 0, value.source);
		token.user_defined = value;
		tokens.push_back(token);
	}

	void emit_eof()
	{
		if (tokens.empty() || tokens.back().kind != PA10TokenKind::End)
			tokens.push_back(PA10Token(PA10TokenKind::End));
	}

	std::vector<PA10Token> tokens;
	bool invalid;
};

struct PA10Name
{
	bool global;
	bool has_decltype_root;
	PA10AstNode decltype_root;
	std::vector<PA10NameComponent> parts;

	PA10Name()
		: global(false), has_decltype_root(false), decltype_root(), parts()
	{}
};

class PA10Parser
{
public:
	PA10Parser(const std::vector<PA10Token>& tokens,
		const PPSpellingTable& producer_spellings)
		: tokens_(tokens), position_(0), work_(0),
		  work_limit_(work_limit_for(tokens.size())), nesting_(0),
		  recursion_depth_(0), angle_depth_(0), non_angle_depth_(0),
		  angle_bases_(),
		  template_close_index_(tokens.size(), tokens.size()),
		  template_top_level_or_(tokens.size(), 0),
		  delimiter_close_index_(tokens.size(), tokens.size()),
		  ast_(), presentation_ids_()
	{
		ast_.snapshot_producer_spellings(producer_spellings);
		build_template_indexes();
	}

	PA10Ast parse()
	{
		if (tokens_.empty() || tokens_.back().kind != PA10TokenKind::End)
			fail("missing PA10 EOF");
		ast_.root = node(PA10NodeKind::TranslationUnit);
		while (!at_end())
			ast_.root.children.push_back(parse_declaration());
		return ast_;
	}

private:
	const std::vector<PA10Token>& tokens_;
	std::size_t position_;
	std::size_t work_;
	std::size_t work_limit_;
	std::size_t nesting_;
	std::size_t recursion_depth_;
	std::size_t angle_depth_;
	std::size_t non_angle_depth_;
	std::vector<std::size_t> angle_bases_;
	std::vector<std::size_t> template_close_index_;
	std::vector<unsigned char> template_top_level_or_;
	std::vector<std::size_t> delimiter_close_index_;
	PA10Ast ast_;
	std::unordered_map<std::string, PA10StringId> presentation_ids_;
	static const std::size_t recursion_limit_ = PA10_MAX_AST_NESTING;

	static std::size_t work_limit_for(std::size_t token_count)
	{
		const std::size_t overhead = 2048;
		const std::size_t per_token = 96;
		const std::size_t maximum = std::numeric_limits<std::size_t>::max();
		if (token_count > (maximum - overhead) / per_token)
			return maximum;
		return token_count * per_token + overhead;
	}

	void fail(const char* message) const
	{
		std::ostringstream text;
		text << message << " at token " << position_;
		throw std::runtime_error(text.str());
	}

	void charge()
	{
		if (work_ >= work_limit_)
			fail("PA10 parser work limit reached");
		++work_;
	}

	void build_template_indexes();

	void enter()
	{
		if (nesting_ >= PA10_MAX_AST_NESTING)
			fail("PA10 parser nesting limit reached");
		charge();
		++nesting_;
	}

	void leave()
	{
		if (nesting_ == 0)
			fail("PA10 parser nesting underflow");
		--nesting_;
	}

	void enter_recursion()
	{
		if (recursion_depth_ >= recursion_limit_)
			fail("PA10 parser recursion limit reached");
		++recursion_depth_;
	}

	void leave_recursion()
	{
		if (recursion_depth_ == 0)
			fail("PA10 parser recursion underflow");
		--recursion_depth_;
	}

	class RecursionGuard
	{
	public:
		explicit RecursionGuard(PA10Parser& parser) : parser_(parser)
		{
			parser_.enter_recursion();
		}

		~RecursionGuard()
		{
			parser_.leave_recursion();
		}

	private:
		PA10Parser& parser_;
	};

	const PA10Token& look(std::size_t offset = 0) const
	{
		if (position_ >= tokens_.size() ||
			offset >= tokens_.size() - position_)
			fail("PA10 parser read past end");
		return tokens_[position_ + offset];
	}

	bool at_end() const { return look().kind == PA10TokenKind::End; }

	bool fixed(SimpleTokenType type, std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::Fixed &&
			look(offset).fixed == type;
	}

	bool identifier(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::Identifier;
	}

	bool literal(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::Literal;
	}

	bool rshift_piece_first(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::RShiftPiece1;
	}

	bool rshift_piece_second(std::size_t offset = 0) const
	{
		return look(offset).kind == PA10TokenKind::RShiftPiece2;
	}

	bool rshift_operator()
	{
		if (!rshift_piece_first())
			return false;
		charge();
		return rshift_piece_second(1);
	}

	bool consume_fixed(SimpleTokenType type)
	{
		if (!fixed(type))
			fail("unexpected fixed token");
		charge();
		++position_;
		return true;
	}

	PA10Token consume_token()
	{
		if (at_end())
			fail("unexpected end of PA10 input");
		charge();
		PA10Token result = look();
		++position_;
		return result;
	}

	void begin_angle()
	{
		if (angle_depth_ >= PA10_MAX_AST_NESTING)
			fail("PA10 parser angle nesting limit reached");
		charge();
		++angle_depth_;
		angle_bases_.push_back(non_angle_depth_);
	}

	bool can_use_angle_operator() const
	{
		return angle_depth_ == 0 ||
			non_angle_depth_ > angle_bases_.back();
	}

	void begin_non_angle()
	{
		if (non_angle_depth_ >= PA10_MAX_AST_NESTING)
			fail("PA10 parser non-angle nesting limit reached");
		charge();
		++non_angle_depth_;
	}

	void end_non_angle()
	{
		if (non_angle_depth_ == 0)
			fail("PA10 parser non-angle nesting underflow");
		--non_angle_depth_;
	}

	bool at_angle_close() const
	{
		return fixed(SimpleTokenType::OP_GT) || rshift_piece_first() ||
			rshift_piece_second();
	}

	void close_angle()
	{
		if (angle_depth_ == 0 || !at_angle_close())
			fail("expected template close angle");
		if (fixed(SimpleTokenType::OP_GT))
			consume_fixed(SimpleTokenType::OP_GT);
		else
			consume_token();
		--angle_depth_;
		angle_bases_.pop_back();
	}

	void consume_shift_operator(PA10Token* spelling)
	{
		if (!rshift_operator())
			fail("expected shift operator");
		const PA10Token first = consume_token();
		const PA10Token second = consume_token();
		if (spelling != NULL)
		{
			*spelling = first;
			spelling->fixed = SimpleTokenType::OP_RSHIFT;
			spelling->source = first.source + second.source;
		}
	}

	PA10StringId intern(const std::string& value)
	{
		std::unordered_map<std::string, PA10StringId>::const_iterator found =
			presentation_ids_.find(value);
		if (found != presentation_ids_.end())
			return found->second;
		const PA10StringId id = ast_.presentation_spellings.size();
		ast_.presentation_spellings.push_back(value);
		presentation_ids_[value] = id;
		return id;
	}

	void append_operator_presentation(PA10AstNode& owner,
		const std::string& value)
	{
		if (owner.operator_presentation_count == 0)
			owner.operator_presentation_begin =
				ast_.operator_presentation_spellings.size();
		ast_.operator_presentation_spellings.push_back(intern(value));
		++owner.operator_presentation_count;
	}

	void append_semantic_child(PA10AstNode& owner, PA10AstNode child)
	{
		if (owner.semantic_child_count == 0)
			owner.semantic_child_begin = ast_.semantic_child_nodes.size();
		ast_.semantic_child_nodes.push_back(std::move(child));
		++owner.semantic_child_count;
	}

	PA10AstNode node(PA10NodeKind kind) const
	{
		return PA10AstNode(kind);
	}

	PA10AstNode fixed_node(PA10NodeKind kind)
	{
		const PA10Token token = consume_token();
		if (token.kind != PA10TokenKind::Fixed)
			fail("expected fixed syntax token");
		PA10AstNode result = node(kind);
		result.has_token = true;
		result.token = token.fixed;
		result.token_spelling = intern(token.source);
		return result;
	}

	PA10AstNode literal_node()
	{
		const PA10Token token = consume_token();
		if (token.kind != PA10TokenKind::Literal)
			fail("expected literal");
		PA10AstNode result = node(PA10NodeKind::Literal);
		result.text = intern(token.source);
		result.has_literal = true;
		result.literal = token.literal;
		return result;
	}

	PA10AstNode name_node(PA10NodeKind kind, const PA10Name& name)
	{
		PA10AstNode result = node(kind);
		result.global_name = name.global;
		result.name_parts = name.parts;
		if (name.has_decltype_root)
		{
			result.name_prefix_begin = ast_.name_prefix_nodes.size();
			result.name_prefix_count = 1;
			ast_.name_prefix_nodes.push_back(name.decltype_root);
		}
		return result;
	}

	PA10Token consume_identifier_token()
	{
		if (!identifier())
			fail("expected identifier");
		return consume_token();
	}

	PPSpellingId consume_identifier()
	{
		return consume_identifier_token().spelling;
	}

	PA10Name parse_name(bool template_expected = false,
		bool allow_standalone_decltype = false);
	PA10NameComponent parse_name_component(bool template_expected,
		bool template_disambiguator);
	void parse_template_arguments(PA10NameComponent& component);
	bool template_suffix_candidate();
	bool find_template_close(std::size_t absolute_lt,
		std::size_t* absolute_close);
	bool template_follow_is_valid(std::size_t absolute_close);
	bool template_declaration_start();
	bool template_argument_starts_type();
	PA10TemplateArgument parse_template_argument();
	bool decltype_qualified_name_start();

	bool name_start(std::size_t offset = 0) const
	{
		return identifier(offset) || fixed(SimpleTokenType::OP_COLON2, offset);
	}

	bool is_decl_specifier(SimpleTokenType type) const
	{
		switch (type)
		{
		case SimpleTokenType::KW_AUTO:
		case SimpleTokenType::KW_BOOL:
		case SimpleTokenType::KW_CHAR:
		case SimpleTokenType::KW_CHAR16_T:
		case SimpleTokenType::KW_CHAR32_T:
		case SimpleTokenType::KW_CONST:
		case SimpleTokenType::KW_CONSTEXPR:
		case SimpleTokenType::KW_DOUBLE:
		case SimpleTokenType::KW_EXTERN:
		case SimpleTokenType::KW_FLOAT:
		case SimpleTokenType::KW_FRIEND:
		case SimpleTokenType::KW_INLINE:
		case SimpleTokenType::KW_INT:
		case SimpleTokenType::KW_LONG:
		case SimpleTokenType::KW_MUTABLE:
		case SimpleTokenType::KW_REGISTER:
		case SimpleTokenType::KW_STATIC:
		case SimpleTokenType::KW_THREAD_LOCAL:
		case SimpleTokenType::KW_TYPEDEF:
		case SimpleTokenType::KW_UNSIGNED:
		case SimpleTokenType::KW_SHORT:
		case SimpleTokenType::KW_SIGNED:
		case SimpleTokenType::KW_VIRTUAL:
		case SimpleTokenType::KW_VOID:
		case SimpleTokenType::KW_VOLATILE:
		case SimpleTokenType::KW_WCHAR_T:
			return true;
		default:
			return false;
		}
	}

	bool is_cv(SimpleTokenType type) const
	{
		return type == SimpleTokenType::KW_CONST ||
			type == SimpleTokenType::KW_VOLATILE;
	}

	bool is_type_keyword(SimpleTokenType type) const
	{
		switch (type)
		{
		case SimpleTokenType::KW_AUTO:
		case SimpleTokenType::KW_BOOL:
		case SimpleTokenType::KW_CHAR:
		case SimpleTokenType::KW_CHAR16_T:
		case SimpleTokenType::KW_CHAR32_T:
		case SimpleTokenType::KW_DOUBLE:
		case SimpleTokenType::KW_FLOAT:
		case SimpleTokenType::KW_INT:
		case SimpleTokenType::KW_LONG:
		case SimpleTokenType::KW_SHORT:
		case SimpleTokenType::KW_SIGNED:
		case SimpleTokenType::KW_UNSIGNED:
		case SimpleTokenType::KW_VOID:
		case SimpleTokenType::KW_WCHAR_T:
			return true;
		default:
			return false;
		}
	}

	bool is_assignment_operator(SimpleTokenType type) const
	{
		switch (type)
		{
		case SimpleTokenType::OP_ASS:
		case SimpleTokenType::OP_PLUSASS:
		case SimpleTokenType::OP_MINUSASS:
		case SimpleTokenType::OP_STARASS:
		case SimpleTokenType::OP_DIVASS:
		case SimpleTokenType::OP_MODASS:
		case SimpleTokenType::OP_XORASS:
		case SimpleTokenType::OP_BANDASS:
		case SimpleTokenType::OP_BORASS:
		case SimpleTokenType::OP_LSHIFTASS:
		case SimpleTokenType::OP_RSHIFTASS:
			return true;
		default:
			return false;
		}
	}

	bool is_operator_function_token(SimpleTokenType type) const
	{
		switch (type)
		{
		case SimpleTokenType::KW_NEW:
		case SimpleTokenType::KW_DELETE:
		case SimpleTokenType::OP_PLUS:
		case SimpleTokenType::OP_MINUS:
		case SimpleTokenType::OP_STAR:
		case SimpleTokenType::OP_DIV:
		case SimpleTokenType::OP_MOD:
		case SimpleTokenType::OP_XOR:
		case SimpleTokenType::OP_AMP:
		case SimpleTokenType::OP_BOR:
		case SimpleTokenType::OP_COMPL:
		case SimpleTokenType::OP_LNOT:
		case SimpleTokenType::OP_ASS:
		case SimpleTokenType::OP_LT:
		case SimpleTokenType::OP_GT:
		case SimpleTokenType::OP_PLUSASS:
		case SimpleTokenType::OP_MINUSASS:
		case SimpleTokenType::OP_STARASS:
		case SimpleTokenType::OP_DIVASS:
		case SimpleTokenType::OP_MODASS:
		case SimpleTokenType::OP_XORASS:
		case SimpleTokenType::OP_BANDASS:
		case SimpleTokenType::OP_BORASS:
		case SimpleTokenType::OP_LSHIFT:
		case SimpleTokenType::OP_RSHIFT:
		case SimpleTokenType::OP_LSHIFTASS:
		case SimpleTokenType::OP_RSHIFTASS:
		case SimpleTokenType::OP_EQ:
		case SimpleTokenType::OP_NE:
		case SimpleTokenType::OP_LE:
		case SimpleTokenType::OP_GE:
		case SimpleTokenType::OP_LAND:
		case SimpleTokenType::OP_LOR:
		case SimpleTokenType::OP_INC:
		case SimpleTokenType::OP_DEC:
		case SimpleTokenType::OP_COMMA:
		case SimpleTokenType::OP_ARROWSTAR:
		case SimpleTokenType::OP_ARROW:
		case SimpleTokenType::OP_DOTSTAR:
			return true;
		default:
			return false;
		}
	}

	bool binary_operator(int level, SimpleTokenType type) const
	{
		switch (level)
		{
		case 0: return type == SimpleTokenType::OP_LOR;
		case 1: return type == SimpleTokenType::OP_LAND;
		case 2: return type == SimpleTokenType::OP_BOR;
		case 3: return type == SimpleTokenType::OP_XOR;
		case 4: return type == SimpleTokenType::OP_AMP;
		case 5: return type == SimpleTokenType::OP_EQ ||
			type == SimpleTokenType::OP_NE;
		case 6: return type == SimpleTokenType::OP_LT ||
			type == SimpleTokenType::OP_GT ||
			type == SimpleTokenType::OP_LE ||
			type == SimpleTokenType::OP_GE;
		case 7: return type == SimpleTokenType::OP_LSHIFT ||
			type == SimpleTokenType::OP_RSHIFT;
		case 8: return type == SimpleTokenType::OP_PLUS ||
			type == SimpleTokenType::OP_MINUS;
		case 9: return type == SimpleTokenType::OP_STAR ||
			type == SimpleTokenType::OP_DIV ||
			type == SimpleTokenType::OP_MOD;
		case 10: return type == SimpleTokenType::OP_DOTSTAR ||
			type == SimpleTokenType::OP_ARROWSTAR;
		default: return false;
		}
	}

	PA10AstNode parse_declaration();
	PA10AstNode parse_namespace(bool inline_namespace);
	PA10AstNode parse_linkage_specification();
	PA10AstNode parse_using();
	PA10AstNode parse_static_assert();
	PA10AstNode parse_template_declaration();
	PA10AstNode parse_class_declaration(bool in_decl_specifier = false);
	PA10AstNode parse_enum_declaration(bool in_decl_specifier = false);
	PA10AstNode parse_class_specifier();
	PA10AstNode parse_enum_specifier();
	PA10AstNode parse_base_specifier();
	PA10AstNode parse_decl_or_function();
	PA10AstNode parse_simple_declaration(PA10AstNode spec,
		PA10AstNode declarator);
	PA10AstNode parse_special_member();
	PA10AstNode parse_operator_name();
	PA10AstNode parse_ctor_initializer();
	PA10AstNode parse_paren_argument_list();
	PA10AstNode parse_decl_specifier_seq(bool type_context = false);
	PA10AstNode parse_type_specifier_seq();
	PA10AstNode parse_decltype_specifier();
	PA10AstNode parse_type_id(bool conversion_target = false);
	PA10AstNode parse_abstract_declarator(
		bool stop_at_empty_parameter_clause = false);
	PA10AstNode parse_declarator(bool allow_abstract = false,
		bool force_parameter_suffix = false);
	PA10AstNode parse_parameter_clause();
	PA10AstNode parse_parameter_declaration();
	PA10AstNode parse_initializer();
	PA10AstNode parse_initializer_clause();
	PA10AstNode parse_braced_init_list();
	PA10AstNode parse_expression();
	PA10AstNode parse_assignment_expression();
	PA10AstNode parse_conditional_expression();
	PA10AstNode parse_binary_expression(int level);
	PA10AstNode parse_unary_expression();
	PA10AstNode parse_unary_expression_base();
	PA10AstNode parse_type_id_or_expression();
	bool looks_like_c_style_cast() const;
	PA10AstNode parse_c_style_cast();
	PA10AstNode parse_type_trait(SimpleTokenType keyword);
	PA10AstNode parse_keyword_cast();
	PA10AstNode parse_new_expression();
	PA10AstNode parse_delete_expression();
	PA10AstNode parse_postfix_expression();
	PA10AstNode parse_primary_expression();
	PA10AstNode parse_lambda_expression();
	PA10AstNode parse_lambda_declarator();
	PA10AstNode parse_argument_list();
	PA10AstNode parse_compound_statement();
	PA10AstNode parse_statement();
	PA10AstNode parse_condition();
	PA10AstNode parse_if_statement();
	PA10AstNode parse_switch_statement();
	PA10AstNode parse_iteration_statement();
	PA10AstNode parse_try_block();
	PA10AstNode parse_class_member();
	PA10AstNode parse_template_parameter_clause();
	PA10AstNode parse_template_parameter();
	void skip_class_key_attribute();

	bool declarator_has_parameter(const PA10AstNode& declarator) const
	{
		if (declarator.kind == PA10NodeKind::ParameterClause)
			return true;
		for (std::size_t i = 0; i < declarator.children.size(); ++i)
			if (declarator_has_parameter(declarator.children[i]))
				return true;
		return false;
	}

	bool looks_like_parameter_clause() const
	{
		if (!fixed(SimpleTokenType::OP_LPAREN))
			return false;
		if (fixed(SimpleTokenType::OP_RPAREN, 1) ||
			fixed(SimpleTokenType::OP_DOTS, 1))
			return true;
		if (is_decl_specifier(look(1).fixed) &&
			look(1).kind == PA10TokenKind::Fixed)
			return true;
		if (identifier(1))
			return fixed(SimpleTokenType::OP_RPAREN, 2) ||
			identifier(2) || fixed(SimpleTokenType::OP_STAR, 2) ||
			fixed(SimpleTokenType::OP_AMP, 2) ||
			fixed(SimpleTokenType::OP_LAND, 2) ||
			fixed(SimpleTokenType::OP_COLON2, 2);
		return false;
	}

	bool declaration_start()
	{
		if (fixed(SimpleTokenType::KW_NAMESPACE) ||
			fixed(SimpleTokenType::KW_USING) ||
			fixed(SimpleTokenType::KW_STATIC_ASSERT) ||
			fixed(SimpleTokenType::KW_TEMPLATE) ||
			fixed(SimpleTokenType::KW_CLASS) ||
			fixed(SimpleTokenType::KW_STRUCT) ||
			fixed(SimpleTokenType::KW_UNION) ||
			fixed(SimpleTokenType::KW_ENUM) ||
			fixed(SimpleTokenType::OP_SEMICOLON))
			return true;
		if (look().kind == PA10TokenKind::Fixed &&
			is_decl_specifier(look().fixed))
			return true;
		return (identifier() && identifier(1)) || template_declaration_start();
	}

	void consume_optional_semicolon()
	{
		if (fixed(SimpleTokenType::OP_SEMICOLON))
			consume_fixed(SimpleTokenType::OP_SEMICOLON);
	}
};

void PA10Parser::build_template_indexes()
{
	std::vector<std::vector<std::size_t> > angle_stacks(1);
	struct DelimiterFrame
	{
		SimpleTokenType kind;
		std::size_t open;

		DelimiterFrame(SimpleTokenType kind, std::size_t open)
			: kind(kind), open(open)
		{}
	};
	std::vector<DelimiterFrame> delimiters;

	for (std::size_t i = 0; i < tokens_.size(); ++i)
	{
		charge();
		const PA10Token& token = tokens_[i];
		if (token.kind == PA10TokenKind::RShiftPiece1 ||
			token.kind == PA10TokenKind::RShiftPiece2)
		{
			if (!angle_stacks.back().empty())
			{
				const std::size_t open = angle_stacks.back().back();
				angle_stacks.back().pop_back();
				template_close_index_[open] = i;
			}
			continue;
		}
		if (token.kind != PA10TokenKind::Fixed)
			continue;

		switch (token.fixed)
		{
		case SimpleTokenType::OP_LPAREN:
			delimiters.push_back(DelimiterFrame(token.fixed, i));
			angle_stacks.push_back(std::vector<std::size_t>());
			break;
		case SimpleTokenType::OP_LSQUARE:
			delimiters.push_back(DelimiterFrame(token.fixed, i));
			angle_stacks.push_back(std::vector<std::size_t>());
			break;
		case SimpleTokenType::OP_LBRACE:
			delimiters.push_back(DelimiterFrame(token.fixed, i));
			angle_stacks.push_back(std::vector<std::size_t>());
			break;
		case SimpleTokenType::OP_RPAREN:
		case SimpleTokenType::OP_RSQUARE:
		case SimpleTokenType::OP_RBRACE:
			if (!delimiters.empty())
			{
				const SimpleTokenType open = delimiters.back().kind;
				const bool matching =
					(open == SimpleTokenType::OP_LPAREN &&
					 token.fixed == SimpleTokenType::OP_RPAREN) ||
					(open == SimpleTokenType::OP_LSQUARE &&
					 token.fixed == SimpleTokenType::OP_RSQUARE) ||
					(open == SimpleTokenType::OP_LBRACE &&
					 token.fixed == SimpleTokenType::OP_RBRACE);
				if (!matching)
					break;
				delimiter_close_index_[delimiters.back().open] = i;
				delimiters.pop_back();
				angle_stacks.pop_back();
			}
			break;
		case SimpleTokenType::OP_LT:
			angle_stacks.back().push_back(i);
			break;
		case SimpleTokenType::OP_GT:
			if (!angle_stacks.back().empty())
			{
				const std::size_t open = angle_stacks.back().back();
				angle_stacks.back().pop_back();
				template_close_index_[open] = i;
			}
			break;
		case SimpleTokenType::OP_LOR:
			if (!angle_stacks.back().empty())
				template_top_level_or_[angle_stacks.back().back()] = 1;
			break;
		default:
			break;
		}
	}
}

bool PA10Parser::find_template_close(std::size_t absolute_lt,
	std::size_t* absolute_close)
{
	if (absolute_lt >= tokens_.size() ||
		tokens_[absolute_lt].kind != PA10TokenKind::Fixed ||
		tokens_[absolute_lt].fixed != SimpleTokenType::OP_LT)
		return false;
	if (template_close_index_[absolute_lt] == tokens_.size())
		return false;
	if (absolute_close != NULL)
		*absolute_close = template_close_index_[absolute_lt];
	return true;
}

bool PA10Parser::template_follow_is_valid(std::size_t absolute_close)
{
	std::size_t next = absolute_close + 1;
	if (next < tokens_.size())
		charge();
	if (absolute_close < tokens_.size() &&
		tokens_[absolute_close].kind == PA10TokenKind::RShiftPiece1 &&
		next < tokens_.size() &&
		tokens_[next].kind == PA10TokenKind::RShiftPiece2)
		return true;
	if (next >= tokens_.size() || tokens_[next].kind == PA10TokenKind::End)
		return true;
	const PA10Token& token = tokens_[next];
	if (token.kind == PA10TokenKind::Identifier ||
		token.kind == PA10TokenKind::Literal)
		return false;
	if (token.kind != PA10TokenKind::Fixed)
		return true;
	switch (token.fixed)
	{
	case SimpleTokenType::OP_LPAREN:
	case SimpleTokenType::OP_LSQUARE:
	case SimpleTokenType::OP_DOT:
	case SimpleTokenType::OP_ARROW:
	case SimpleTokenType::OP_COLON2:
	case SimpleTokenType::OP_SEMICOLON:
	case SimpleTokenType::OP_COMMA:
	case SimpleTokenType::OP_RPAREN:
	case SimpleTokenType::OP_RSQUARE:
	case SimpleTokenType::OP_RBRACE:
	case SimpleTokenType::OP_LT:
	case SimpleTokenType::OP_GT:
	case SimpleTokenType::OP_LE:
	case SimpleTokenType::OP_GE:
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
	case SimpleTokenType::OP_STAR:
	case SimpleTokenType::OP_DIV:
	case SimpleTokenType::OP_MOD:
	case SimpleTokenType::OP_AMP:
	case SimpleTokenType::OP_BOR:
	case SimpleTokenType::OP_XOR:
	case SimpleTokenType::OP_LAND:
	case SimpleTokenType::OP_LOR:
	case SimpleTokenType::OP_QMARK:
	case SimpleTokenType::OP_COLON:
	case SimpleTokenType::OP_ASS:
		return true;
	default:
		return false;
	}
}

bool PA10Parser::decltype_qualified_name_start()
{
	if (!fixed(SimpleTokenType::KW_DECLTYPE))
		return false;
	charge();
	if (!fixed(SimpleTokenType::OP_LPAREN, 1))
		return false;
	const std::size_t open = position_ + 1;
	if (open >= delimiter_close_index_.size())
		return false;
	const std::size_t close = delimiter_close_index_[open];
	if (close >= tokens_.size() || close + 1 >= tokens_.size())
		return false;
	charge();
	return tokens_[close + 1].kind == PA10TokenKind::Fixed &&
		tokens_[close + 1].fixed == SimpleTokenType::OP_COLON2;
}

bool PA10Parser::template_suffix_candidate()
{
	if (!fixed(SimpleTokenType::OP_LT))
		return false;
	std::size_t close = 0;
	if (!find_template_close(position_, &close) ||
		!template_follow_is_valid(close))
		return false;
	return !template_top_level_or_[position_] ||
		close + 1 >= tokens_.size() ||
		tokens_[close + 1].kind != PA10TokenKind::Fixed ||
		tokens_[close + 1].fixed != SimpleTokenType::OP_LPAREN;
}

PA10NameComponent PA10Parser::parse_name_component(bool template_expected,
	bool template_disambiguator)
{
	if (!identifier())
		fail("expected name component");
	PA10NameComponent component;
	component.spelling = consume_identifier();
	component.template_disambiguator = template_disambiguator;
	if (fixed(SimpleTokenType::OP_LT))
	{
		if (!template_expected && !template_disambiguator &&
			!template_suffix_candidate())
			return component;
		parse_template_arguments(component);
	}
	else if (template_disambiguator)
		fail("template disambiguator needs a template-id");
	return component;
}

void PA10Parser::parse_template_arguments(PA10NameComponent& component)
{
	consume_fixed(SimpleTokenType::OP_LT);
	begin_angle();
	component.has_template_id = true;
	std::vector<PA10TemplateArgument> direct_arguments;
	if (!at_angle_close())
	{
		while (true)
		{
			direct_arguments.push_back(parse_template_argument());
			if (!fixed(SimpleTokenType::OP_COMMA))
				break;
			consume_fixed(SimpleTokenType::OP_COMMA);
			if (at_angle_close())
				fail("trailing template argument comma");
		}
	}
	close_angle();
	component.template_argument_begin = ast_.template_arguments.size();
	component.template_argument_count = direct_arguments.size();
	for (std::size_t i = 0; i < direct_arguments.size(); ++i)
		ast_.template_arguments.push_back(std::move(direct_arguments[i]));
}

bool PA10Parser::template_argument_starts_type()
{
	if (look().kind == PA10TokenKind::Fixed)
	{
		if (is_type_keyword(look().fixed) || is_cv(look().fixed) ||
			look().fixed == SimpleTokenType::KW_TYPENAME ||
			look().fixed == SimpleTokenType::KW_DECLTYPE)
			return true;
	}
	if (!identifier())
		return false;
	charge();
	return fixed(SimpleTokenType::OP_STAR, 1) ||
		fixed(SimpleTokenType::OP_AMP, 1) ||
		fixed(SimpleTokenType::OP_LAND, 1) ||
		fixed(SimpleTokenType::OP_LSQUARE, 1);
}

PA10TemplateArgument PA10Parser::parse_template_argument()
{
	if (template_argument_starts_type())
	{
		const PA10TemplateArgumentKind kind = identifier() ?
			PA10TemplateArgumentKind::Unresolved :
			PA10TemplateArgumentKind::TypeId;
		return PA10TemplateArgument(kind, parse_type_id());
	}
	if (identifier() || name_start())
		return PA10TemplateArgument(PA10TemplateArgumentKind::Unresolved,
			parse_assignment_expression());
	return PA10TemplateArgument(PA10TemplateArgumentKind::Expression,
		parse_assignment_expression());
}

PA10Name PA10Parser::parse_name(bool template_expected,
	bool allow_standalone_decltype)
{
	PA10Name result;
	if (fixed(SimpleTokenType::KW_DECLTYPE))
	{
		result.has_decltype_root = true;
		result.decltype_root = parse_decltype_specifier();
		if (fixed(SimpleTokenType::OP_COLON2))
			consume_fixed(SimpleTokenType::OP_COLON2);
		else if (allow_standalone_decltype)
			return result;
		else
			fail("decltype root requires qualified-name separator");
	}
	else if (fixed(SimpleTokenType::OP_COLON2))
	{
		result.global = true;
		consume_fixed(SimpleTokenType::OP_COLON2);
	}
	while (true)
	{
		bool template_disambiguator = false;
		if (fixed(SimpleTokenType::KW_TEMPLATE))
		{
			consume_fixed(SimpleTokenType::KW_TEMPLATE);
			template_disambiguator = true;
		}
		result.parts.push_back(parse_name_component(template_expected,
			template_disambiguator));
		if (!fixed(SimpleTokenType::OP_COLON2))
			break;
		consume_fixed(SimpleTokenType::OP_COLON2);
		if (!identifier() && !fixed(SimpleTokenType::KW_TEMPLATE))
			fail("missing qualified-name component");
	}
	return result;
}

bool PA10Parser::template_declaration_start()
{
	if (!identifier())
		return false;
	std::size_t offset = 0;
	while (offset < tokens_.size() - position_ &&
		tokens_[position_ + offset].kind == PA10TokenKind::Identifier)
	{
		charge();
		if (offset + 1 < tokens_.size() - position_ &&
			tokens_[position_ + offset + 1].kind == PA10TokenKind::Fixed &&
			tokens_[position_ + offset + 1].fixed == SimpleTokenType::OP_LT)
		{
			std::size_t close = 0;
			if (!find_template_close(position_ + offset + 1, &close))
				return false;
			std::size_t next = close + 1;
			if (tokens_[close].kind == PA10TokenKind::RShiftPiece1 &&
				next < tokens_.size() &&
				tokens_[next].kind == PA10TokenKind::RShiftPiece2)
			{
				charge();
				++next;
			}
			if (next >= tokens_.size())
				return false;
			charge();
			return next < tokens_.size() &&
				tokens_[next].kind == PA10TokenKind::Identifier;
		}
		if (offset + 1 >= tokens_.size() - position_ ||
			tokens_[position_ + offset + 1].kind != PA10TokenKind::Fixed ||
			tokens_[position_ + offset + 1].fixed != SimpleTokenType::OP_COLON2)
			break;
		offset += 2;
	}
	return false;
}

PA10AstNode PA10Parser::parse_declaration()
{
	RecursionGuard recursion(*this);
	if (fixed(SimpleTokenType::OP_SEMICOLON))
	{
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::EmptyDeclaration);
	}
	if (fixed(SimpleTokenType::KW_INLINE))
	{
		if (fixed(SimpleTokenType::KW_NAMESPACE, 1))
		{
			consume_fixed(SimpleTokenType::KW_INLINE);
			return parse_namespace(true);
		}
	}
	if (fixed(SimpleTokenType::KW_NAMESPACE))
		return parse_namespace(false);
	if (fixed(SimpleTokenType::KW_EXTERN) && literal(1))
		return parse_linkage_specification();
	if (fixed(SimpleTokenType::KW_USING))
		return parse_using();
	if (fixed(SimpleTokenType::KW_STATIC_ASSERT))
		return parse_static_assert();
	if (fixed(SimpleTokenType::KW_TEMPLATE))
		return parse_template_declaration();
	if (fixed(SimpleTokenType::KW_CLASS) ||
		fixed(SimpleTokenType::KW_STRUCT) || fixed(SimpleTokenType::KW_UNION))
		return parse_class_declaration();
	if (fixed(SimpleTokenType::KW_ENUM))
		return parse_enum_declaration();
	return parse_decl_or_function();
}

PA10AstNode PA10Parser::parse_namespace(bool inline_namespace)
{
	consume_fixed(SimpleTokenType::KW_NAMESPACE);
	PPSpellingId producer_name = 0;
	bool anonymous = true;
	if (identifier())
	{
		const PA10Token token = consume_identifier_token();
		producer_name = token.spelling;
		anonymous = false;
	}
	if (fixed(SimpleTokenType::OP_ASS))
	{
		if (anonymous)
			fail("unnamed namespace alias");
		consume_fixed(SimpleTokenType::OP_ASS);
		const PA10Name target = parse_name(true);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		PA10AstNode result = node(PA10NodeKind::NamespaceAliasDefinition);
		result.producer_spelling = producer_name;
		result.children.push_back(name_node(PA10NodeKind::Target, target));
		return result;
	}
	consume_fixed(SimpleTokenType::OP_LBRACE);
	PA10AstNode result = node(PA10NodeKind::NamespaceDefinition);
	if (anonymous)
		result.text = intern("<unnamed>");
	else
	{
		result.producer_spelling = producer_name;
	}
	if (inline_namespace)
		result.children.push_back(node(PA10NodeKind::InlineMarker));
	enter();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (at_end())
			fail("unterminated namespace");
		result.children.push_back(parse_declaration());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave();
	return result;
}

PA10AstNode PA10Parser::parse_linkage_specification()
{
	consume_fixed(SimpleTokenType::KW_EXTERN);
	if (!literal())
		fail("linkage specification needs a string literal");
	const PA10Token linkage = consume_token();
	PA10AstNode result = node(PA10NodeKind::LinkageSpecification);
	result.has_literal = true;
	result.literal = linkage.literal;
	consume_fixed(SimpleTokenType::OP_LBRACE);
	enter();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (at_end())
			fail("unterminated linkage specification");
		result.children.push_back(parse_declaration());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave();
	return result;
}

PA10AstNode PA10Parser::parse_using()
{
	consume_fixed(SimpleTokenType::KW_USING);
	if (fixed(SimpleTokenType::KW_NAMESPACE))
	{
		consume_fixed(SimpleTokenType::KW_NAMESPACE);
		PA10AstNode result = node(PA10NodeKind::UsingDirective);
		result.children.push_back(name_node(PA10NodeKind::Target,
			parse_name(true)));
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	const PA10Name name = parse_name(true);
	if (fixed(SimpleTokenType::OP_ASS))
	{
		if (name.global || name.parts.size() != 1)
			fail("qualified alias name");
		consume_fixed(SimpleTokenType::OP_ASS);
		PA10AstNode result = node(PA10NodeKind::AliasDeclaration);
		result.producer_spelling = name.parts.back().spelling;
		result.children.push_back(parse_type_id());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	PA10AstNode result = node(PA10NodeKind::UsingDeclaration);
	result.children.push_back(name_node(PA10NodeKind::Target, name));
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}

PA10AstNode PA10Parser::parse_static_assert()
{
	consume_fixed(SimpleTokenType::KW_STATIC_ASSERT);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::StaticAssertDeclaration);
	result.children.push_back(parse_assignment_expression());
	if (fixed(SimpleTokenType::OP_COMMA))
	{
		consume_fixed(SimpleTokenType::OP_COMMA);
		if (!literal())
			fail("static_assert message must be a literal");
		const PA10Token token = consume_token();
		PA10AstNode message = node(PA10NodeKind::Message);
		message.text = intern(token.source);
		message.has_literal = true;
		message.literal = token.literal;
		result.children.push_back(std::move(message));
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}

PA10AstNode PA10Parser::parse_decl_or_function()
{
	PA10AstNode spec = parse_decl_specifier_seq();
	if (fixed(SimpleTokenType::OP_SEMICOLON))
	{
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		PA10AstNode result = node(PA10NodeKind::SimpleDeclaration);
		result.children.push_back(std::move(spec));
		return result;
	}
	PA10AstNode declarator = parse_declarator(false);
	const bool has_parameter = declarator_has_parameter(declarator);
	if (fixed(SimpleTokenType::OP_LBRACE) &&
		has_parameter)
	{
		PA10AstNode result = node(PA10NodeKind::FunctionDefinition);
		result.children.push_back(std::move(spec));
		result.children.push_back(std::move(declarator));
		result.children.push_back(parse_compound_statement());
		return result;
	}
	return parse_simple_declaration(spec, declarator);
}

PA10AstNode PA10Parser::parse_simple_declaration(PA10AstNode spec,
	PA10AstNode declarator)
{
	PA10AstNode result = node(PA10NodeKind::SimpleDeclaration);
	result.children.push_back(std::move(spec));
	PA10AstNode list = node(PA10NodeKind::InitDeclaratorList);
	PA10AstNode init = node(PA10NodeKind::InitDeclarator);
	const bool has_parameter = declarator_has_parameter(declarator);
	init.children.push_back(std::move(declarator));
	if (fixed(SimpleTokenType::OP_ASS) || fixed(SimpleTokenType::OP_LBRACE) ||
		(fixed(SimpleTokenType::OP_LPAREN) && !has_parameter))
		init.children.push_back(parse_initializer());
	list.children.push_back(std::move(init));
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		consume_fixed(SimpleTokenType::OP_COMMA);
		PA10AstNode next = node(PA10NodeKind::InitDeclarator);
		next.children.push_back(parse_declarator(false));
		if (fixed(SimpleTokenType::OP_ASS) || fixed(SimpleTokenType::OP_LBRACE) ||
			(fixed(SimpleTokenType::OP_LPAREN) &&
			 !declarator_has_parameter(next.children.back())))
			next.children.push_back(parse_initializer());
		list.children.push_back(std::move(next));
	}
	result.children.push_back(std::move(list));
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}

PA10AstNode PA10Parser::parse_decl_specifier_seq(bool type_context)
{
	PA10AstNode result = node(type_context ? PA10NodeKind::TypeSpecifierSeq :
		PA10NodeKind::DeclSpecifierSeq);
	bool consumed = false;
	bool saw_type = false;
	while (true)
	{
		if (type_context && fixed(SimpleTokenType::KW_TYPENAME))
		{
			const PA10Token keyword = consume_token();
			const PA10Name name = parse_name(true);
			PA10AstNode child = name_node(PA10NodeKind::TypeName, name);
			child.has_token = true;
			child.token = keyword.fixed;
			child.token_spelling = intern(keyword.source);
			result.children.push_back(std::move(child));
			consumed = true;
			saw_type = true;
			continue;
		}
		if (fixed(SimpleTokenType::KW_DECLTYPE) && !saw_type)
		{
			result.children.push_back(parse_decltype_specifier());
			consumed = true;
			saw_type = true;
			continue;
		}
		if (name_start() && !saw_type)
		{
			const PA10Name name = parse_name(true);
			PA10AstNode child = name_node(type_context ? PA10NodeKind::TypeName :
				PA10NodeKind::DeclSpecifier, name);
			if (!type_context && !name.global && name.parts.size() == 1 &&
				!name.parts.front().has_template_id)
				child.identifier_declspecifier = true;
			result.children.push_back(std::move(child));
			consumed = true;
			saw_type = true;
			continue;
		}
		if (fixed(SimpleTokenType::KW_CLASS) ||
			fixed(SimpleTokenType::KW_STRUCT) || fixed(SimpleTokenType::KW_UNION))
		{
			if (saw_type)
				break;
			result.children.push_back(parse_class_specifier());
			consumed = true;
			saw_type = true;
			continue;
		}
		if (fixed(SimpleTokenType::KW_ENUM))
		{
			if (saw_type)
				break;
			result.children.push_back(parse_enum_specifier());
			consumed = true;
			saw_type = true;
			continue;
		}
		if (look().kind != PA10TokenKind::Fixed ||
			!is_decl_specifier(look().fixed))
			break;
		const SimpleTokenType spec_type = look().fixed;
		const PA10NodeKind kind = type_context ?
			(is_cv(spec_type) ? PA10NodeKind::CvQualifier :
			 PA10NodeKind::TypeSpecifier) : PA10NodeKind::DeclSpecifier;
		result.children.push_back(fixed_node(kind));
		consumed = true;
		if (is_type_keyword(spec_type))
			saw_type = true;
	}
	if (!consumed)
		fail("missing declaration specifier");
	return result;
}

PA10AstNode PA10Parser::parse_type_specifier_seq()
{
	return parse_decl_specifier_seq(true);
}

PA10AstNode PA10Parser::parse_decltype_specifier()
{
	PA10AstNode result = node(PA10NodeKind::DecltypeSpecifier);
	const PA10Token keyword = consume_token();
	if (keyword.kind != PA10TokenKind::Fixed ||
		keyword.fixed != SimpleTokenType::KW_DECLTYPE)
		fail("expected decltype");
	result.has_token = true;
	result.token = keyword.fixed;
	result.token_spelling = intern(keyword.source);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	begin_non_angle();
	result.children.push_back(parse_expression());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	end_non_angle();
	return result;
}

PA10AstNode PA10Parser::parse_type_id(bool conversion_target)
{
	PA10AstNode result = node(PA10NodeKind::TypeId);
	result.children.push_back(parse_type_specifier_seq());
	if (fixed(SimpleTokenType::OP_STAR) || fixed(SimpleTokenType::OP_AMP) ||
		fixed(SimpleTokenType::OP_LAND) || fixed(SimpleTokenType::OP_LSQUARE) ||
		(fixed(SimpleTokenType::OP_LPAREN) &&
		 (fixed(SimpleTokenType::OP_STAR, 1) ||
		  fixed(SimpleTokenType::OP_AMP, 1) ||
		  fixed(SimpleTokenType::OP_LAND, 1) ||
		  fixed(SimpleTokenType::OP_LPAREN, 1))))
		result.children.push_back(parse_abstract_declarator(conversion_target));
	return result;
}

PA10AstNode PA10Parser::parse_abstract_declarator(
	bool stop_at_empty_parameter_clause)
{
	RecursionGuard recursion(*this);
	PA10AstNode result = node(PA10NodeKind::AbstractDeclarator);
	while (fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND))
	{
		PA10AstNode pointer = fixed_node(PA10NodeKind::PtrOperator);
		result.children.push_back(std::move(pointer));
		while (is_cv(look().fixed) && look().kind == PA10TokenKind::Fixed)
			result.children.push_back(fixed_node(PA10NodeKind::CvQualifier));
	}
	if (fixed(SimpleTokenType::OP_LPAREN) &&
		!(stop_at_empty_parameter_clause &&
			fixed(SimpleTokenType::OP_RPAREN, 1)))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		PA10AstNode nested = node(PA10NodeKind::NestedDeclarator);
		nested.children.push_back(parse_abstract_declarator());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		result.children.push_back(std::move(nested));
	}
	while (fixed(SimpleTokenType::OP_LSQUARE))
	{
		consume_fixed(SimpleTokenType::OP_LSQUARE);
		PA10AstNode suffix = node(PA10NodeKind::ArraySuffix);
		if (!fixed(SimpleTokenType::OP_RSQUARE))
			suffix.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RSQUARE);
		result.children.push_back(std::move(suffix));
	}
	return result;
}

PA10AstNode PA10Parser::parse_declarator(bool allow_abstract,
	bool force_parameter_suffix)
{
	enter();
	PA10AstNode result = node(PA10NodeKind::Declarator);
	while (fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND))
	{
		PA10AstNode pointer = fixed_node(PA10NodeKind::PtrOperator);
		result.children.push_back(std::move(pointer));
		while (is_cv(look().fixed) && look().kind == PA10TokenKind::Fixed)
			result.children.push_back(fixed_node(PA10NodeKind::CvQualifier));
	}
	if (name_start())
	{
		result.children.push_back(name_node(PA10NodeKind::Identifier,
			parse_name(true)));
	}
	else if (fixed(SimpleTokenType::KW_OPERATOR))
	{
		result.children.push_back(parse_operator_name());
	}
	else if (fixed(SimpleTokenType::OP_COMPL))
	{
		const PA10Token tilde = consume_token();
		const PA10Token name = consume_identifier_token();
		PA10AstNode identifier_node = node(PA10NodeKind::Identifier);
		identifier_node.unqualified_id_kind =
			PA10UnqualifiedIdKind::Destructor;
		identifier_node.unqualified_id_token = tilde.fixed;
		identifier_node.unqualified_id_token_spelling = intern(tilde.source);
		identifier_node.unqualified_id_spelling = name.spelling;
		result.children.push_back(std::move(identifier_node));
	}
	else if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		PA10AstNode nested = node(PA10NodeKind::NestedDeclarator);
		nested.children.push_back(parse_declarator(true));
		consume_fixed(SimpleTokenType::OP_RPAREN);
		result.children.push_back(std::move(nested));
	}
	else if (!allow_abstract)
		fail("expected declarator-id");

	while (fixed(SimpleTokenType::OP_LPAREN) ||
		fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (fixed(SimpleTokenType::OP_LPAREN) &&
			(force_parameter_suffix || looks_like_parameter_clause()))
			result.children.push_back(parse_parameter_clause());
		else if (fixed(SimpleTokenType::OP_LPAREN))
			break;
		else
		{
			consume_fixed(SimpleTokenType::OP_LSQUARE);
			PA10AstNode suffix = node(PA10NodeKind::ArraySuffix);
			if (!fixed(SimpleTokenType::OP_RSQUARE))
				suffix.children.push_back(parse_expression());
			consume_fixed(SimpleTokenType::OP_RSQUARE);
			result.children.push_back(std::move(suffix));
		}
	}

	while (is_cv(look().fixed) && look().kind == PA10TokenKind::Fixed)
		result.children.push_back(fixed_node(PA10NodeKind::CvQualifier));
	while (fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND))
		result.children.push_back(fixed_node(PA10NodeKind::RefQualifier));
	if (fixed(SimpleTokenType::KW_NOEXCEPT))
	{
		PA10AstNode qualifier = fixed_node(PA10NodeKind::FunctionQualifier);
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			consume_fixed(SimpleTokenType::OP_LPAREN);
			qualifier.children.push_back(parse_expression());
			consume_fixed(SimpleTokenType::OP_RPAREN);
		}
		result.children.push_back(std::move(qualifier));
	}
	if (fixed(SimpleTokenType::OP_ARROW))
	{
		consume_fixed(SimpleTokenType::OP_ARROW);
		PA10AstNode trailing = node(PA10NodeKind::TrailingReturnType);
		PA10AstNode type = parse_type_id();
		if (!type.children.empty())
		{
			const PA10AstNode& seq = type.children.front();
			if (!seq.children.empty())
			{
				const PA10AstNode& first = seq.children.back();
				if (first.name_prefix_count != 0 || !first.name_parts.empty())
				{
					trailing.name_prefix_begin = first.name_prefix_begin;
						trailing.name_prefix_count = first.name_prefix_count;
						trailing.name_parts = first.name_parts;
					}
				}
		}
		trailing.children.push_back(std::move(type));
		result.children.push_back(std::move(trailing));
	}
	leave();
	return result;
}

PA10AstNode PA10Parser::parse_parameter_clause()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::ParameterClause);
	if (fixed(SimpleTokenType::OP_RPAREN))
	{
		consume_fixed(SimpleTokenType::OP_RPAREN);
		return result;
	}
	if (fixed(SimpleTokenType::OP_DOTS))
	{
		consume_fixed(SimpleTokenType::OP_DOTS);
		result.children.push_back(node(PA10NodeKind::ParameterPack));
		consume_fixed(SimpleTokenType::OP_RPAREN);
		return result;
	}
	while (true)
	{
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			consume_fixed(SimpleTokenType::OP_DOTS);
			result.children.push_back(node(PA10NodeKind::ParameterPack));
			break;
		}
		result.children.push_back(parse_parameter_declaration());
		if (fixed(SimpleTokenType::OP_RPAREN))
			break;
		consume_fixed(SimpleTokenType::OP_COMMA);
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			consume_fixed(SimpleTokenType::OP_DOTS);
			result.children.push_back(node(PA10NodeKind::ParameterPack));
			break;
		}
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	return result;
}

PA10AstNode PA10Parser::parse_parameter_declaration()
{
	PA10AstNode result = node(PA10NodeKind::ParameterDeclaration);
	result.children.push_back(parse_decl_specifier_seq());
	if (!fixed(SimpleTokenType::OP_COMMA) &&
		!fixed(SimpleTokenType::OP_RPAREN) &&
		!fixed(SimpleTokenType::OP_DOTS))
		result.children.push_back(parse_declarator(true));
	if (fixed(SimpleTokenType::OP_ASS))
	{
		PA10AstNode argument = node(PA10NodeKind::DefaultArgument);
		argument.children.push_back(parse_initializer());
		result.children.push_back(std::move(argument));
	}
	return result;
}

PA10AstNode PA10Parser::parse_initializer()
{
	PA10AstNode result = node(PA10NodeKind::Initializer);
	bool had_equal = false;
	if (fixed(SimpleTokenType::OP_ASS))
	{
		consume_fixed(SimpleTokenType::OP_ASS);
		had_equal = true;
		if (fixed(SimpleTokenType::KW_DEFAULT) ||
			fixed(SimpleTokenType::KW_DELETE))
		{
			PA10AstNode special_initializer =
				fixed_node(PA10NodeKind::SpecialInitializer);
			result.children.push_back(std::move(special_initializer));
			return result;
		}
	}
	if (fixed(SimpleTokenType::OP_LBRACE))
		result.children.push_back(parse_braced_init_list());
	else if (fixed(SimpleTokenType::OP_LPAREN) && !had_equal)
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		begin_non_angle();
		PA10AstNode paren = node(PA10NodeKind::ParenInitializer);
		if (!fixed(SimpleTokenType::OP_RPAREN))
		{
			paren.children.push_back(parse_initializer_clause());
			while (fixed(SimpleTokenType::OP_COMMA))
			{
				consume_fixed(SimpleTokenType::OP_COMMA);
				paren.children.push_back(parse_initializer_clause());
			}
		}
		consume_fixed(SimpleTokenType::OP_RPAREN);
		end_non_angle();
		result.children.push_back(std::move(paren));
	}
	else
		result.children.push_back(parse_initializer_clause());
	return result;
}

PA10AstNode PA10Parser::parse_initializer_clause()
{
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_braced_init_list();
	return parse_assignment_expression();
}

PA10AstNode PA10Parser::parse_braced_init_list()
{
	RecursionGuard recursion(*this);
	consume_fixed(SimpleTokenType::OP_LBRACE);
	PA10AstNode result = node(PA10NodeKind::BracedInitList);
	if (!fixed(SimpleTokenType::OP_RBRACE))
	{
		result.children.push_back(parse_initializer_clause());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			if (fixed(SimpleTokenType::OP_RBRACE))
				break;
			result.children.push_back(parse_initializer_clause());
		}
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	return result;
}

PA10AstNode PA10Parser::parse_expression()
{
	RecursionGuard recursion(*this);
	PA10AstNode result = parse_assignment_expression();
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		const PA10Token op = consume_token();
		PA10AstNode right = parse_assignment_expression();
		PA10AstNode combined = node(PA10NodeKind::BinaryExpression);
		combined.has_token = true;
		combined.token = op.fixed;
		combined.token_spelling = intern(op.source);
		combined.children.push_back(std::move(result));
		combined.children.push_back(std::move(right));
		result = std::move(combined);
	}
	return result;
}

PA10AstNode PA10Parser::parse_assignment_expression()
{
	RecursionGuard recursion(*this);
	PA10AstNode result = parse_conditional_expression();
	if (look().kind == PA10TokenKind::Fixed &&
		is_assignment_operator(look().fixed))
	{
		const PA10Token op = consume_token();
		PA10AstNode assignment = node(PA10NodeKind::AssignmentExpression);
		assignment.has_token = true;
		assignment.token = op.fixed;
		assignment.token_spelling = intern(op.source);
		assignment.children.push_back(std::move(result));
		assignment.children.push_back(parse_assignment_expression());
		return assignment;
	}
	return result;
}

PA10AstNode PA10Parser::parse_conditional_expression()
{
	PA10AstNode result = parse_binary_expression(0);
	if (fixed(SimpleTokenType::OP_QMARK))
	{
		consume_fixed(SimpleTokenType::OP_QMARK);
		PA10AstNode conditional = node(PA10NodeKind::ConditionalExpression);
		conditional.children.push_back(std::move(result));
		conditional.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_COLON);
		conditional.children.push_back(parse_assignment_expression());
		return conditional;
	}
	return result;
}

PA10AstNode PA10Parser::parse_binary_expression(int level)
{
	PA10AstNode result;
	if (level == 10)
		result = parse_unary_expression();
	else
		result = parse_binary_expression(level + 1);
	while ((look().kind == PA10TokenKind::Fixed &&
			binary_operator(level, look().fixed)) ||
		(level == 7 && rshift_operator()))
	{
		if (level == 6 && fixed(SimpleTokenType::OP_GT) &&
			!can_use_angle_operator())
			break;
		const bool is_shift = level == 7 && rshift_operator();
		if (is_shift && !can_use_angle_operator())
			break;
		if (level == 7 && !is_shift &&
			!(look().kind == PA10TokenKind::Fixed &&
				look().fixed == SimpleTokenType::OP_LSHIFT))
			break;
		PA10Token op;
		if (is_shift)
			consume_shift_operator(&op);
		else
			op = consume_token();
		PA10AstNode binary = node(PA10NodeKind::BinaryExpression);
		binary.has_token = true;
		binary.token = op.fixed;
		binary.token_spelling = intern(op.source);
		binary.children.push_back(std::move(result));
		if (level == 10)
			binary.children.push_back(parse_unary_expression());
		else
			binary.children.push_back(parse_binary_expression(level + 1));
		result = std::move(binary);
	}
	return result;
}

bool PA10Parser::looks_like_c_style_cast() const
{
	if (!fixed(SimpleTokenType::OP_LPAREN))
		return false;
	const bool simple_type = look(1).kind == PA10TokenKind::Fixed &&
		(is_type_keyword(look(1).fixed) || is_cv(look(1).fixed));
	const bool named_type = identifier(1) &&
		(fixed(SimpleTokenType::OP_RPAREN, 2) ||
		 fixed(SimpleTokenType::OP_STAR, 2) ||
		 fixed(SimpleTokenType::OP_AMP, 2) ||
		 fixed(SimpleTokenType::OP_LAND, 2));
	if (!simple_type && !named_type)
		return false;
	std::size_t close = simple_type || named_type ? 2 : 0;
	if (fixed(SimpleTokenType::OP_STAR, 2) ||
		fixed(SimpleTokenType::OP_AMP, 2) ||
		fixed(SimpleTokenType::OP_LAND, 2))
		return false;
	if (!fixed(SimpleTokenType::OP_RPAREN, close))
		return false;
	const PA10TokenKind next_kind = look(close + 1).kind;
	if (next_kind == PA10TokenKind::Identifier ||
		next_kind == PA10TokenKind::Literal)
		return true;
	return fixed(SimpleTokenType::OP_LPAREN, close + 1) ||
		fixed(SimpleTokenType::KW_TRUE, close + 1) ||
		fixed(SimpleTokenType::KW_FALSE, close + 1) ||
		fixed(SimpleTokenType::KW_NULLPTR, close + 1);
}

PA10AstNode PA10Parser::parse_c_style_cast()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::CastExpression);
	result.has_token = true;
	result.token = SimpleTokenType::OP_LPAREN;
	// The checked dump treats this synthetic cast marker as an enum-only
	// boundary and intentionally leaves its source spelling empty.
	result.token_spelling = 0;
	result.children.push_back(parse_type_id());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	result.children.push_back(parse_unary_expression());
	return result;
}

PA10AstNode PA10Parser::parse_unary_expression()
{
	RecursionGuard recursion(*this);
	std::vector<PA10Token> prefixes;
	while (look().kind == PA10TokenKind::Fixed &&
		(fixed(SimpleTokenType::OP_INC) || fixed(SimpleTokenType::OP_DEC) ||
			fixed(SimpleTokenType::OP_STAR) || fixed(SimpleTokenType::OP_AMP) ||
			fixed(SimpleTokenType::OP_PLUS) || fixed(SimpleTokenType::OP_MINUS) ||
			fixed(SimpleTokenType::OP_LNOT) || fixed(SimpleTokenType::OP_COMPL)))
		prefixes.push_back(consume_token());

	PA10AstNode result = parse_unary_expression_base();
	for (std::size_t i = prefixes.size(); i != 0; --i)
	{
		const PA10Token& op = prefixes[i - 1];
		PA10AstNode unary = node(PA10NodeKind::UnaryExpression);
		unary.has_token = true;
		unary.token = op.fixed;
		unary.token_spelling = intern(op.source);
		unary.children.push_back(std::move(result));
		result = std::move(unary);
	}
	return result;
}

PA10AstNode PA10Parser::parse_unary_expression_base()
{
	if (looks_like_c_style_cast())
		return parse_c_style_cast();
	if (fixed(SimpleTokenType::KW_SIZEOF))
	{
		consume_fixed(SimpleTokenType::KW_SIZEOF);
		PA10AstNode result = node(PA10NodeKind::SizeofExpression);
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			consume_fixed(SimpleTokenType::OP_LPAREN);
			result.children.push_back(parse_type_id_or_expression());
			consume_fixed(SimpleTokenType::OP_RPAREN);
		}
		else
			result.children.push_back(parse_unary_expression());
		return result;
	}
	if (fixed(SimpleTokenType::KW_ALIGNOF))
		return parse_type_trait(SimpleTokenType::KW_ALIGNOF);
	if (fixed(SimpleTokenType::KW_NOEXCEPT))
		return parse_type_trait(SimpleTokenType::KW_NOEXCEPT);
	if (fixed(SimpleTokenType::KW_TYPEID))
		return parse_type_trait(SimpleTokenType::KW_TYPEID);
	if (fixed(SimpleTokenType::KW_STATIC_CAST) ||
		fixed(SimpleTokenType::KW_DYNAMIC_CAST) ||
		fixed(SimpleTokenType::KW_CONST_CAST) ||
		fixed(SimpleTokenType::KW_REINTERPET_CAST))
		return parse_keyword_cast();
	if (fixed(SimpleTokenType::KW_NEW))
		return parse_new_expression();
	if (fixed(SimpleTokenType::KW_DELETE))
		return parse_delete_expression();
	return parse_postfix_expression();
}

PA10AstNode PA10Parser::parse_type_id_or_expression()
{
	if ((look().kind == PA10TokenKind::Fixed &&
		is_type_keyword(look().fixed)) ||
		(is_cv(look().fixed) && look().kind == PA10TokenKind::Fixed))
		return parse_type_id();
	if (identifier() && (fixed(SimpleTokenType::OP_AMP, 1) ||
		fixed(SimpleTokenType::OP_LAND, 1) ||
		fixed(SimpleTokenType::OP_STAR, 1)))
		return parse_type_id();
	return parse_expression();
}

PA10AstNode PA10Parser::parse_type_trait(SimpleTokenType keyword)
{
	PA10AstNode result = node(PA10NodeKind::TypeTraitExpression);
	result.has_token = true;
	const PA10Token token = consume_token();
	if (token.kind != PA10TokenKind::Fixed || token.fixed != keyword)
		fail("unexpected type-trait keyword");
	result.token = token.fixed;
	result.token_spelling = intern(token.source);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	result.children.push_back(parse_type_id_or_expression());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	return result;
}

PA10AstNode PA10Parser::parse_keyword_cast()
{
	const PA10Token keyword = consume_token();
	PA10AstNode result = node(PA10NodeKind::CastExpression);
	result.has_token = true;
	result.token = keyword.fixed;
	result.token_spelling = intern(keyword.source);
	consume_fixed(SimpleTokenType::OP_LT);
	begin_angle();
	result.children.push_back(parse_type_id());
	close_angle();
	consume_fixed(SimpleTokenType::OP_LPAREN);
	result.children.push_back(parse_expression());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	return result;
}

PA10AstNode PA10Parser::parse_new_expression()
{
	consume_fixed(SimpleTokenType::KW_NEW);
	PA10AstNode result = node(PA10NodeKind::NewExpression);
	result.children.push_back(parse_type_id());
	if (fixed(SimpleTokenType::OP_LPAREN) ||
		fixed(SimpleTokenType::OP_LBRACE))
		result.children.push_back(parse_initializer());
	return result;
}

PA10AstNode PA10Parser::parse_delete_expression()
{
	consume_fixed(SimpleTokenType::KW_DELETE);
	PA10AstNode result = node(PA10NodeKind::DeleteExpression);
	if (fixed(SimpleTokenType::OP_LSQUARE))
	{
		consume_fixed(SimpleTokenType::OP_LSQUARE);
		consume_fixed(SimpleTokenType::OP_RSQUARE);
		result.children.push_back(node(PA10NodeKind::ArrayDeleteMarker));
	}
	result.children.push_back(parse_unary_expression());
	return result;
}

PA10AstNode PA10Parser::parse_postfix_expression()
{
	PA10AstNode result = parse_primary_expression();
	while (true)
	{
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			PA10AstNode call = node(PA10NodeKind::CallExpression);
			call.children.push_back(std::move(result));
			call.children.push_back(parse_argument_list());
			result = std::move(call);
			continue;
		}
		if (fixed(SimpleTokenType::OP_LSQUARE))
		{
			consume_fixed(SimpleTokenType::OP_LSQUARE);
			PA10AstNode subscript = node(PA10NodeKind::SubscriptExpression);
			subscript.children.push_back(std::move(result));
			subscript.children.push_back(parse_expression());
			consume_fixed(SimpleTokenType::OP_RSQUARE);
			result = std::move(subscript);
			continue;
		}
		if (fixed(SimpleTokenType::OP_DOT) ||
			fixed(SimpleTokenType::OP_ARROW))
		{
			const PA10Token op = consume_token();
			PA10AstNode member = node(PA10NodeKind::MemberExpression);
			member.has_token = true;
			member.token = op.fixed;
			member.token_spelling = intern(op.source);
			member.children.push_back(std::move(result));
			if (!name_start() && !fixed(SimpleTokenType::KW_TEMPLATE))
				fail("expected member name");
			member.children.push_back(name_node(PA10NodeKind::Identifier,
				parse_name()));
			result = std::move(member);
			continue;
		}
		if (fixed(SimpleTokenType::OP_INC) || fixed(SimpleTokenType::OP_DEC))
		{
			const PA10Token op = consume_token();
			PA10AstNode postfix = node(PA10NodeKind::PostfixExpression);
			postfix.has_token = true;
			postfix.token = op.fixed;
			postfix.token_spelling = intern(op.source);
			postfix.children.push_back(std::move(result));
			result = std::move(postfix);
			continue;
		}
		break;
	}
	return result;
}

PA10AstNode PA10Parser::parse_primary_expression()
{
	RecursionGuard recursion(*this);
	if (literal())
		return literal_node();
	if (fixed(SimpleTokenType::KW_TRUE) ||
		fixed(SimpleTokenType::KW_FALSE) ||
		fixed(SimpleTokenType::KW_NULLPTR) ||
		fixed(SimpleTokenType::KW_THIS))
	{
		const PA10Token token = consume_token();
		PA10AstNode result = node(PA10NodeKind::KeywordLiteral);
		result.has_token = true;
		result.token = token.fixed;
		result.token_spelling = intern(token.source);
		return result;
	}
	if (name_start() || decltype_qualified_name_start())
		return name_node(PA10NodeKind::IdExpression, parse_name());
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		begin_non_angle();
		PA10AstNode result = node(PA10NodeKind::ParenthesizedExpression);
		result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		end_non_angle();
		return result;
	}
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_braced_init_list();
	if (fixed(SimpleTokenType::OP_LSQUARE))
		return parse_lambda_expression();
	fail("expected primary expression");
	return node(PA10NodeKind::Literal);
}

PA10AstNode PA10Parser::parse_lambda_expression()
{
	consume_fixed(SimpleTokenType::OP_LSQUARE);
	PA10AstNode result = node(PA10NodeKind::LambdaExpression);
	PA10AstNode introducer = node(PA10NodeKind::LambdaIntroducer);
	introducer.text = intern("[]");
	if (!fixed(SimpleTokenType::OP_RSQUARE))
		fail("only empty lambda capture is in the active PA10 slice");
	consume_fixed(SimpleTokenType::OP_RSQUARE);
	result.children.push_back(std::move(introducer));
	if (fixed(SimpleTokenType::OP_LPAREN))
		result.children.push_back(parse_lambda_declarator());
	result.children.push_back(parse_compound_statement());
	return result;
}

PA10AstNode PA10Parser::parse_lambda_declarator()
{
	PA10AstNode result = node(PA10NodeKind::LambdaDeclarator);
	result.children.push_back(parse_parameter_clause());
	if (fixed(SimpleTokenType::KW_MUTABLE))
		consume_fixed(SimpleTokenType::KW_MUTABLE);
	if (fixed(SimpleTokenType::KW_NOEXCEPT))
	{
		PA10AstNode qualifier = fixed_node(PA10NodeKind::FunctionQualifier);
		result.children.push_back(std::move(qualifier));
	}
	if (fixed(SimpleTokenType::OP_ARROW))
	{
		consume_fixed(SimpleTokenType::OP_ARROW);
		PA10AstNode trailing = node(PA10NodeKind::TrailingReturnType);
		trailing.children.push_back(parse_type_id());
		result.children.push_back(std::move(trailing));
	}
	return result;
}

PA10AstNode PA10Parser::parse_argument_list()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	begin_non_angle();
	PA10AstNode result = node(PA10NodeKind::ArgumentList);
	if (!fixed(SimpleTokenType::OP_RPAREN))
	{
		result.children.push_back(parse_initializer_clause());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			result.children.push_back(parse_initializer_clause());
		}
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	end_non_angle();
	return result;
}

PA10AstNode PA10Parser::parse_compound_statement()
{
	RecursionGuard recursion(*this);
	consume_fixed(SimpleTokenType::OP_LBRACE);
	PA10AstNode result = node(PA10NodeKind::CompoundStatement);
	enter();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (at_end())
			fail("unterminated compound statement");
		result.children.push_back(parse_statement());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave();
	return result;
}

PA10AstNode PA10Parser::parse_statement()
{
	RecursionGuard recursion(*this);
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_compound_statement();
	if (fixed(SimpleTokenType::KW_RETURN))
	{
		consume_fixed(SimpleTokenType::KW_RETURN);
		PA10AstNode result = node(PA10NodeKind::ReturnStatement);
		if (!fixed(SimpleTokenType::OP_SEMICOLON))
			result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	if (fixed(SimpleTokenType::KW_IF))
		return parse_if_statement();
	if (fixed(SimpleTokenType::KW_SWITCH))
		return parse_switch_statement();
	if (fixed(SimpleTokenType::KW_WHILE) || fixed(SimpleTokenType::KW_DO) ||
		fixed(SimpleTokenType::KW_FOR))
		return parse_iteration_statement();
	if (fixed(SimpleTokenType::KW_TRY))
		return parse_try_block();
	if (fixed(SimpleTokenType::KW_BREAK))
	{
		consume_fixed(SimpleTokenType::KW_BREAK);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::BreakStatement);
	}
	if (fixed(SimpleTokenType::KW_CONTINUE))
	{
		consume_fixed(SimpleTokenType::KW_CONTINUE);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::ContinueStatement);
	}
	if (fixed(SimpleTokenType::KW_GOTO))
	{
		consume_fixed(SimpleTokenType::KW_GOTO);
		PA10AstNode result = node(PA10NodeKind::GotoStatement);
		const PA10Token label = consume_identifier_token();
		result.producer_spelling = label.spelling;
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	if (fixed(SimpleTokenType::KW_THROW))
	{
		consume_fixed(SimpleTokenType::KW_THROW);
		PA10AstNode result = node(PA10NodeKind::ThrowStatement);
		if (!fixed(SimpleTokenType::OP_SEMICOLON))
			result.children.push_back(parse_assignment_expression());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	if (fixed(SimpleTokenType::KW_CASE))
	{
		consume_fixed(SimpleTokenType::KW_CASE);
		PA10AstNode result = node(PA10NodeKind::CaseStatement);
		result.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_statement());
		return result;
	}
	if (fixed(SimpleTokenType::KW_DEFAULT))
	{
		consume_fixed(SimpleTokenType::KW_DEFAULT);
		PA10AstNode result = node(PA10NodeKind::DefaultStatement);
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_statement());
		return result;
	}
	if (identifier() && fixed(SimpleTokenType::OP_COLON, 1))
	{
		PA10AstNode result = node(PA10NodeKind::LabeledStatement);
		const PA10Token label = consume_identifier_token();
		result.producer_spelling = label.spelling;
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_statement());
		return result;
	}
	if (declaration_start())
		return parse_declaration();
	PA10AstNode result = node(PA10NodeKind::ExpressionStatement);
	if (!fixed(SimpleTokenType::OP_SEMICOLON))
		result.children.push_back(parse_expression());
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}

PA10AstNode PA10Parser::parse_condition()
{
	PA10AstNode result = node(PA10NodeKind::Condition);
	const bool declaration =
		(look().kind == PA10TokenKind::Fixed && is_type_keyword(look().fixed)) ||
		(identifier() && identifier(1));
	if (declaration)
	{
		PA10AstNode condition = node(PA10NodeKind::ConditionDeclaration);
		condition.children.push_back(parse_decl_specifier_seq());
		condition.children.push_back(parse_declarator(false));
		if (fixed(SimpleTokenType::OP_ASS) || fixed(SimpleTokenType::OP_LBRACE) ||
			fixed(SimpleTokenType::OP_LPAREN))
			condition.children.push_back(parse_initializer());
		else
			fail("condition declaration needs initializer");
		result.children.push_back(std::move(condition));
	}
	else
		result.children.push_back(parse_expression());
	return result;
}

PA10AstNode PA10Parser::parse_if_statement()
{
	consume_fixed(SimpleTokenType::KW_IF);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::IfStatement);
	result.children.push_back(parse_condition());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	PA10AstNode then_branch = node(PA10NodeKind::ThenBranch);
	then_branch.children.push_back(parse_statement());
	result.children.push_back(std::move(then_branch));
	if (fixed(SimpleTokenType::KW_ELSE))
	{
		consume_fixed(SimpleTokenType::KW_ELSE);
		PA10AstNode else_branch = node(PA10NodeKind::ElseBranch);
		else_branch.children.push_back(parse_statement());
		result.children.push_back(std::move(else_branch));
	}
	return result;
}

PA10AstNode PA10Parser::parse_switch_statement()
{
	consume_fixed(SimpleTokenType::KW_SWITCH);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::SwitchStatement);
	result.children.push_back(parse_condition());
	consume_fixed(SimpleTokenType::OP_RPAREN);
	result.children.push_back(parse_statement());
	return result;
}

PA10AstNode PA10Parser::parse_iteration_statement()
{
	if (fixed(SimpleTokenType::KW_WHILE))
	{
		consume_fixed(SimpleTokenType::KW_WHILE);
		consume_fixed(SimpleTokenType::OP_LPAREN);
		PA10AstNode result = node(PA10NodeKind::WhileStatement);
		result.children.push_back(parse_condition());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		result.children.push_back(parse_statement());
		return result;
	}
	if (fixed(SimpleTokenType::KW_DO))
	{
		consume_fixed(SimpleTokenType::KW_DO);
		PA10AstNode result = node(PA10NodeKind::DoStatement);
		result.children.push_back(parse_statement());
		consume_fixed(SimpleTokenType::KW_WHILE);
		consume_fixed(SimpleTokenType::OP_LPAREN);
		result.children.push_back(node(PA10NodeKind::Condition));
		result.children.back().children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_RPAREN);
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	consume_fixed(SimpleTokenType::KW_FOR);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	PA10AstNode result = node(PA10NodeKind::ForStatement);
	PA10AstNode init = node(PA10NodeKind::ForInitStatement);
	if (declaration_start())
		init.children.push_back(parse_declaration());
	else if (!fixed(SimpleTokenType::OP_SEMICOLON))
	{
		init.children.push_back(parse_expression());
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
	}
	else
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
	result.children.push_back(std::move(init));
	if (!fixed(SimpleTokenType::OP_SEMICOLON))
		result.children.push_back(parse_condition());
	else
		result.children.push_back(node(PA10NodeKind::Condition));
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	if (!fixed(SimpleTokenType::OP_RPAREN))
	{
		PA10AstNode iteration = node(PA10NodeKind::Iteration);
		iteration.children.push_back(parse_expression());
		result.children.push_back(std::move(iteration));
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	result.children.push_back(parse_statement());
	return result;
}

PA10AstNode PA10Parser::parse_try_block()
{
	consume_fixed(SimpleTokenType::KW_TRY);
	PA10AstNode result = node(PA10NodeKind::TryBlock);
	result.children.push_back(parse_compound_statement());
	while (fixed(SimpleTokenType::KW_CATCH))
	{
		consume_fixed(SimpleTokenType::KW_CATCH);
		consume_fixed(SimpleTokenType::OP_LPAREN);
		PA10AstNode handler = node(PA10NodeKind::Handler);
		PA10AstNode exception = node(PA10NodeKind::ExceptionDeclaration);
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			const PA10Token ellipsis = consume_token();
			PA10AstNode leaf = node(PA10NodeKind::LeafFixed);
			leaf.has_token = true;
			leaf.token = ellipsis.fixed;
			leaf.token_spelling = intern(ellipsis.source);
			exception.children.push_back(std::move(leaf));
		}
		else
		{
			exception.children.push_back(parse_decl_specifier_seq());
			if (!fixed(SimpleTokenType::OP_RPAREN))
				exception.children.push_back(parse_declarator(true));
		}
		handler.children.push_back(std::move(exception));
		consume_fixed(SimpleTokenType::OP_RPAREN);
		handler.children.push_back(parse_compound_statement());
		result.children.push_back(std::move(handler));
	}
	if (result.children.size() == 1)
		fail("try block needs handler");
	return result;
}

PA10AstNode PA10Parser::parse_class_declaration(bool in_decl_specifier)
{
	(void)in_decl_specifier;
	const SimpleTokenType key = look().fixed;
	if ((fixed(key) && identifier(1) &&
		fixed(SimpleTokenType::OP_SEMICOLON, 2)) ||
		(fixed(key) && fixed(SimpleTokenType::OP_SEMICOLON, 1)))
	{
		PA10AstNode result = node(PA10NodeKind::ClassForwardDeclaration);
		result.text = intern("<unnamed>");
		result.children.push_back(fixed_node(PA10NodeKind::ClassKey));
		if (identifier())
		{
			const PA10Token name = consume_identifier_token();
			result.producer_spelling = name.spelling;
		}
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return result;
	}
	PA10AstNode result = parse_class_specifier();
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}

PA10AstNode PA10Parser::parse_class_specifier()
{
	if (!(fixed(SimpleTokenType::KW_CLASS) ||
		fixed(SimpleTokenType::KW_STRUCT) || fixed(SimpleTokenType::KW_UNION)))
		fail("expected class-key");
	PA10AstNode result = node(PA10NodeKind::ClassSpecifier);
	PA10AstNode key = fixed_node(PA10NodeKind::ClassKey);
	result.children.push_back(std::move(key));
	skip_class_key_attribute();
	if (identifier())
	{
		const PA10Name name = parse_name(true);
		if (!name.global && name.parts.size() == 1 &&
			!name.parts.front().has_template_id)
			result.producer_spelling = name.parts.front().spelling;
		else
		{
			result.global_name = name.global;
			result.name_parts = name.parts;
		}
	}
	else
		result.text = intern("<unnamed>");
	if (fixed(SimpleTokenType::OP_COLON))
	{
		consume_fixed(SimpleTokenType::OP_COLON);
		PA10AstNode base = node(PA10NodeKind::BaseClause);
		base.children.push_back(parse_base_specifier());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			base.children.push_back(parse_base_specifier());
		}
		result.children.push_back(std::move(base));
	}
	consume_fixed(SimpleTokenType::OP_LBRACE);
	enter();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (at_end())
			fail("unterminated class specifier");
		result.children.push_back(parse_class_member());
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave();
	return result;
}

void PA10Parser::skip_class_key_attribute()
{
	if (!fixed(SimpleTokenType::KW_ALIGNAS))
		return;
	consume_fixed(SimpleTokenType::KW_ALIGNAS);
	consume_fixed(SimpleTokenType::OP_LPAREN);
	std::size_t depth = 1;
	while (depth != 0)
	{
		if (at_end())
			fail("unterminated class-key attribute");
		if (fixed(SimpleTokenType::OP_LPAREN))
			++depth;
		else if (fixed(SimpleTokenType::OP_RPAREN))
			--depth;
		consume_token();
	}
}

PA10AstNode PA10Parser::parse_base_specifier()
{
	PA10AstNode result = node(PA10NodeKind::BaseSpecifier);
	if (fixed(SimpleTokenType::KW_VIRTUAL))
		result.children.push_back(fixed_node(PA10NodeKind::VirtualSpecifier));
	if (fixed(SimpleTokenType::KW_PUBLIC) ||
		fixed(SimpleTokenType::KW_PROTECTED) ||
		fixed(SimpleTokenType::KW_PRIVATE))
		result.children.push_back(fixed_node(PA10NodeKind::AccessSpecifier));
	if (fixed(SimpleTokenType::KW_VIRTUAL))
		result.children.push_back(fixed_node(PA10NodeKind::VirtualSpecifier));
	if (!name_start() && !fixed(SimpleTokenType::KW_DECLTYPE))
		fail("expected base name");
	result.children.push_back(name_node(PA10NodeKind::BaseName,
		parse_name(true, true)));
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return result;
}

PA10AstNode PA10Parser::parse_operator_name()
{
	const PA10Token keyword = consume_token();
	if (keyword.kind != PA10TokenKind::Fixed ||
		keyword.fixed != SimpleTokenType::KW_OPERATOR)
		fail("expected operator keyword");

	PA10AstNode result = node(PA10NodeKind::Identifier);
	result.unqualified_id_kind = PA10UnqualifiedIdKind::OperatorFunction;
	result.unqualified_id_token = keyword.fixed;
	append_operator_presentation(result, keyword.source);
	if (fixed(SimpleTokenType::OP_LSQUARE))
	{
		const PA10Token open = consume_token();
		if (!fixed(SimpleTokenType::OP_RSQUARE))
			fail("operator[] needs closing bracket");
		const PA10Token close = consume_token();
		result.operator_function_kind = PA10OperatorFunctionKind::Subscript;
		result.operator_token = open.fixed;
		append_operator_presentation(result, open.source);
		append_operator_presentation(result, close.source);
	}
	else if (fixed(SimpleTokenType::OP_LPAREN))
	{
		const PA10Token open = consume_token();
		if (!fixed(SimpleTokenType::OP_RPAREN))
			fail("operator() needs closing parenthesis");
		const PA10Token close = consume_token();
		result.operator_function_kind = PA10OperatorFunctionKind::Call;
		result.operator_token = open.fixed;
		append_operator_presentation(result, open.source);
		append_operator_presentation(result, close.source);
	}
	else if (rshift_operator())
	{
		const PA10Token first = consume_token();
		const PA10Token second = consume_token();
		result.operator_function_kind = PA10OperatorFunctionKind::Token;
		result.operator_token = SimpleTokenType::OP_RSHIFT;
		append_operator_presentation(result, first.source);
		append_operator_presentation(result, second.source);
	}
	else if (look().kind == PA10TokenKind::Fixed &&
		is_operator_function_token(look().fixed))
	{
		const PA10Token op = consume_token();
		result.operator_function_kind = op.fixed == SimpleTokenType::KW_NEW ?
			PA10OperatorFunctionKind::New :
			op.fixed == SimpleTokenType::KW_DELETE ?
			PA10OperatorFunctionKind::Delete :
			PA10OperatorFunctionKind::Token;
		result.operator_token = op.fixed;
		append_operator_presentation(result, op.source);
		if ((op.fixed == SimpleTokenType::KW_NEW ||
			op.fixed == SimpleTokenType::KW_DELETE) &&
			fixed(SimpleTokenType::OP_LSQUARE))
		{
			const PA10Token open = consume_token();
			if (!fixed(SimpleTokenType::OP_RSQUARE))
				fail("allocation operator[] needs closing bracket");
			const PA10Token close = consume_token();
			result.operator_function_kind = op.fixed == SimpleTokenType::KW_NEW ?
				PA10OperatorFunctionKind::NewArray :
				PA10OperatorFunctionKind::DeleteArray;
			append_operator_presentation(result, open.source);
			append_operator_presentation(result, close.source);
		}
	}
	else
	{
		const std::size_t begin = position_;
		PA10AstNode conversion_type = parse_type_id(true);
		result.operator_function_kind = PA10OperatorFunctionKind::Conversion;
		append_semantic_child(result, std::move(conversion_type));
		for (std::size_t i = begin; i < position_; ++i)
			append_operator_presentation(result, tokens_[i].source);
	}
	return result;
}

PA10AstNode PA10Parser::parse_paren_argument_list()
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	begin_non_angle();
	PA10AstNode result = node(PA10NodeKind::ParenArgumentList);
	if (!fixed(SimpleTokenType::OP_RPAREN))
	{
		result.children.push_back(parse_initializer_clause());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			result.children.push_back(parse_initializer_clause());
		}
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	end_non_angle();
	return result;
}

PA10AstNode PA10Parser::parse_ctor_initializer()
{
	consume_fixed(SimpleTokenType::OP_COLON);
	PA10AstNode result = node(PA10NodeKind::CtorInitializer);
	while (true)
	{
		if (!name_start() && !fixed(SimpleTokenType::KW_DECLTYPE))
			fail("expected mem-initializer-id");
		PA10AstNode initializer = node(PA10NodeKind::MemInitializer);
		initializer.children.push_back(
			name_node(PA10NodeKind::MemInitializerId,
				parse_name(true, true)));
		if (fixed(SimpleTokenType::OP_LPAREN))
			initializer.children.push_back(parse_paren_argument_list());
		else if (fixed(SimpleTokenType::OP_LBRACE))
			initializer.children.push_back(parse_braced_init_list());
		else
			fail("mem-initializer needs an argument list");
		result.children.push_back(std::move(initializer));
		if (!fixed(SimpleTokenType::OP_COMMA))
			break;
		consume_fixed(SimpleTokenType::OP_COMMA);
	}
	return result;
}

PA10AstNode PA10Parser::parse_special_member()
{
	PA10AstNode declarator = parse_declarator(false, true);
	PA10AstNode result = node(PA10NodeKind::SpecialMemberDeclaration);
	result.children.push_back(std::move(declarator));

	if (fixed(SimpleTokenType::OP_COLON))
		result.children.push_back(parse_ctor_initializer());

	if (fixed(SimpleTokenType::OP_LBRACE))
	{
		result.kind = PA10NodeKind::SpecialMemberDefinition;
		result.children.push_back(parse_compound_statement());
		return result;
	}

	if (fixed(SimpleTokenType::OP_ASS))
		result.children.push_back(parse_initializer());
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	result.kind = PA10NodeKind::SpecialMemberDeclaration;
	return result;
}

PA10AstNode PA10Parser::parse_class_member()
{
	if (fixed(SimpleTokenType::KW_PUBLIC) ||
		fixed(SimpleTokenType::KW_PROTECTED) ||
		fixed(SimpleTokenType::KW_PRIVATE))
	{
		PA10AstNode result = fixed_node(PA10NodeKind::AccessSpecifier);
		consume_fixed(SimpleTokenType::OP_COLON);
		return result;
	}
	if (fixed(SimpleTokenType::OP_SEMICOLON))
	{
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return node(PA10NodeKind::EmptyDeclaration);
	}
	if (fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT) ||
		fixed(SimpleTokenType::KW_UNION))
		return parse_class_declaration();
	if (fixed(SimpleTokenType::KW_ENUM))
		return parse_enum_declaration();
	if (fixed(SimpleTokenType::KW_OPERATOR) ||
		fixed(SimpleTokenType::OP_COMPL) ||
		(identifier() && fixed(SimpleTokenType::OP_LPAREN, 1)))
		return parse_special_member();
	return parse_declaration();
}

PA10AstNode PA10Parser::parse_enum_declaration(bool in_decl_specifier)
{
	(void)in_decl_specifier;
	PA10AstNode result = parse_enum_specifier();
	if (!fixed(SimpleTokenType::OP_SEMICOLON))
		fail("enum declaration needs semicolon");
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	return result;
}

PA10AstNode PA10Parser::parse_enum_specifier()
{
	consume_fixed(SimpleTokenType::KW_ENUM);
	PA10AstNode result = node(PA10NodeKind::EnumSpecifier);
	if (fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT))
		result.children.push_back(fixed_node(PA10NodeKind::EnumKey));
	if (identifier())
	{
		const PA10Token name = consume_identifier_token();
		result.producer_spelling = name.spelling;
	}
	if (fixed(SimpleTokenType::OP_COLON))
	{
		consume_fixed(SimpleTokenType::OP_COLON);
		result.children.push_back(parse_type_id());
	}
	if (!fixed(SimpleTokenType::OP_LBRACE))
		return result;
	consume_fixed(SimpleTokenType::OP_LBRACE);
	if (!fixed(SimpleTokenType::OP_RBRACE))
	{
		while (true)
		{
			if (!identifier())
				fail("expected enumerator");
			PA10AstNode enumerator = node(PA10NodeKind::Enumerator);
			const PA10Token name = consume_identifier_token();
			enumerator.producer_spelling = name.spelling;
			if (fixed(SimpleTokenType::OP_ASS))
			{
				consume_fixed(SimpleTokenType::OP_ASS);
				enumerator.children.push_back(parse_assignment_expression());
			}
			result.children.push_back(std::move(enumerator));
			if (!fixed(SimpleTokenType::OP_COMMA))
				break;
			consume_fixed(SimpleTokenType::OP_COMMA);
			if (fixed(SimpleTokenType::OP_RBRACE))
				break;
		}
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	return result;
}

PA10AstNode PA10Parser::parse_template_declaration()
{
	consume_fixed(SimpleTokenType::KW_TEMPLATE);
	PA10AstNode result = node(PA10NodeKind::TemplateDeclaration);
	result.children.push_back(parse_template_parameter_clause());
	result.children.push_back(parse_declaration());
	return result;
}

PA10AstNode PA10Parser::parse_template_parameter_clause()
{
	consume_fixed(SimpleTokenType::OP_LT);
	begin_angle();
	PA10AstNode result = node(PA10NodeKind::TemplateParameterClause);
	if (!at_angle_close())
	{
		PA10AstNode list = node(PA10NodeKind::TemplateParameterList);
		list.children.push_back(parse_template_parameter());
		while (fixed(SimpleTokenType::OP_COMMA))
		{
			consume_fixed(SimpleTokenType::OP_COMMA);
			list.children.push_back(parse_template_parameter());
		}
		result.children.push_back(std::move(list));
	}
	close_angle();
	return result;
}

PA10AstNode PA10Parser::parse_template_parameter()
{
	if (fixed(SimpleTokenType::KW_CLASS) ||
		fixed(SimpleTokenType::KW_TYPENAME))
	{
		PA10AstNode result = node(PA10NodeKind::TypeParameter);
		result.children.push_back(fixed_node(PA10NodeKind::ParameterKey));
		if (fixed(SimpleTokenType::OP_DOTS))
			result.children.push_back(fixed_node(PA10NodeKind::ParameterPack));
		if (identifier())
		{
			PA10AstNode id = node(PA10NodeKind::Identifier);
			const PA10Token name = consume_identifier_token();
			id.producer_spelling = name.spelling;
			result.children.push_back(std::move(id));
		}
		if (fixed(SimpleTokenType::OP_ASS))
		{
			consume_fixed(SimpleTokenType::OP_ASS);
			PA10AstNode argument = node(PA10NodeKind::DefaultTemplateArgument);
			argument.children.push_back(parse_type_id());
			result.children.push_back(std::move(argument));
		}
		return result;
	}
	PA10AstNode result = node(PA10NodeKind::NonTypeTemplateParameter);
	result.children.push_back(parse_decl_specifier_seq());
	if (name_start() || fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND) ||
		fixed(SimpleTokenType::OP_LPAREN))
		result.children.push_back(parse_declarator(true));
	if (fixed(SimpleTokenType::OP_ASS))
	{
		consume_fixed(SimpleTokenType::OP_ASS);
		PA10AstNode argument = node(PA10NodeKind::DefaultArgument);
		argument.children.push_back(parse_initializer());
		result.children.push_back(std::move(argument));
	}
	return result;
}

} // namespace


PA10Ast parse_pa10_ast(const PPTokenBuffer& input)
{
	PA10PostTokenCollector collector;
	posttokenize_cpp_tokens(input, collector);
	if (collector.invalid)
		throw std::runtime_error("invalid phase-7 token");
	PA10Parser parser(collector.tokens, input.spellings);
	return parser.parse();
}
