#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

void Pa15Lowerer::index_global_symbols(){
		for (std::size_t scope_index = 0; scope_index < model_.scopes_.size(); ++scope_index)
		{
			const Scope& scope = model_.scopes_[scope_index];
			const bool namespace_scope = scope.kind == ScopeKind::Namespace;
			const bool class_scope = scope.kind == ScopeKind::Class;
			if (!namespace_scope && !class_scope) continue;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId binding_id = scope.bindings[i];
				const Binding& binding = model_.binding(binding_id);
				const bool class_static = class_scope &&
					model_.is_static_member(binding_id);
				if (binding.kind != BindingKind::Variable ||
					(class_scope && !class_static) ||
					(class_static && !binding.has_definition &&
						(binding_id.value >= required_global_bindings_.size() ||
						 required_global_bindings_[binding_id.value] == 0)))
					continue;
				const std::string internal_name = internal_value_name(ScopeId(scope_index), binding.name);
				global_symbols_[binding_id.value] = SymbolId(next_symbol_++);
				global_name_ids_[binding_id.value] = symbol_spelling(internal_name);
				symbol_name_ids_[global_symbols_[binding_id.value].index] =
					global_name_ids_[binding_id.value];
			}
		}
}

void Pa15Lowerer::collect_globals(){
		index_global_symbols();
		for (std::size_t scope_index = 0; scope_index < model_.scopes_.size(); ++scope_index)
		{
			const Scope& scope = model_.scopes_[scope_index];
			const bool namespace_scope = scope.kind == ScopeKind::Namespace;
			const bool class_scope = scope.kind == ScopeKind::Class;
			if (!namespace_scope && !class_scope) continue;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId binding_id = scope.bindings[i];
				const Binding& binding = model_.binding(binding_id);
				const bool class_static = class_scope &&
					model_.is_static_member(binding_id);
				if (binding.kind != BindingKind::Variable ||
					(class_scope && !class_static) ||
					(class_static && !binding.has_definition &&
						(binding_id.value >= required_global_bindings_.size() ||
						 required_global_bindings_[binding_id.value] == 0)))
					continue;
				const DeclarationFact* declaration = NULL;
				std::map<std::size_t, const DeclarationFact*>::const_iterator declaration_it =
					declaration_by_binding_.find(binding_id.value);
				if (declaration_it != declaration_by_binding_.end()) declaration = declaration_it->second;
				if (class_static && declaration == NULL)
					throw std::runtime_error("PA15 static member declaration is missing");
				std::size_t source_declaration, source_declarator;
				global_declaration_position(binding_id, declaration, &source_declaration,
					&source_declarator);
				const SymbolId symbol = global_symbols_.find(binding_id.value)->second;
				const SpellingId name_id = global_name_ids_.find(binding_id.value)->second;
				const SpellingId object_name = intern_spelling(abi_variable_symbol(
					binding_id, ScopeId(scope_index)));
				const bool unknown_array = model_.type_kind(model_.strip_cv_type(binding.type)) ==
					TypeKind::Array && model_.types_[model_.strip_cv_type(binding.type).value].unknown_bound;
				const std::vector<SemanticFactId> initializers =
					variable_facts_.find(binding_id.value) != variable_facts_.end() ?
					children(variable_facts_.find(binding_id.value)->second) :
					std::vector<SemanticFactId>();
				const std::map<std::size_t, bool>::const_iterator thread_local_it =
					thread_local_by_binding_.find(binding_id.value);
				const bool is_thread_local = thread_local_it !=
					thread_local_by_binding_.end() && thread_local_it->second;
				bool has_tls_construction = false;
				const bool declaration_only = !binding.has_definition;
				if (declaration_only)
				{
					GlobalDeclaration entry;
					entry.symbol_id = symbol;
					entry.name_id = name_id;
					// Class-static declarations are ABI boundaries, not storage.
					entry.has_type = !class_static && !unknown_array;
					if (entry.has_type) entry.type = low_type(binding.type);
					if (is_thread_local)
						entry.storage = lowir_model::GSM_THREAD_LOCAL;
					entry.metadata.binding = binding.internal_linkage ? lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
					entry.metadata.object_symbol_id = object_name;
					if (binding.language_linkage == LanguageLinkage::C)
						entry.metadata.linkage = lowir_model::LLM_C;
					program_.global_declarations.push_back(entry);
					append_tls_wrapper(binding_id, ScopeId(scope_index), name_id);
					continue;
				}
				GlobalDefinition entry;
				entry.symbol_id = symbol;
				entry.name_id = name_id;
				entry.metadata.binding = binding.internal_linkage ? lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
				entry.metadata.object_symbol_id = object_name;
				if (binding.language_linkage == LanguageLinkage::C)
					entry.metadata.linkage = lowir_model::LLM_C;
				if (is_thread_local)
					entry.storage = lowir_model::GSM_THREAD_LOCAL;
				TypeId object_type = model_.strip_cv_type(binding.type);
				if (model_.type_kind(object_type) == TypeKind::Array)
				{
					if (model_.types_[object_type.value].unknown_bound)
						throw std::runtime_error("PA15 unknown-bound array needs an extern declaration");
					const LowType entry_type = low_type(object_type);
					entry.structured = true;
					if (initializers.empty())
					{
						GlobalDefinition::DataItem zero;
						zero.kind = GlobalDefinition::DataItem::ITEM_ZERO;
						zero.zero_bytes = entry_type.storage_size();
						entry.data_items.push_back(zero);
					}
					else
					{
						if (initializers.size() != 1)
							throw std::runtime_error(
								"PA15 global aggregate initializer arity is invalid");
						GlobalDefinition candidate = entry;
						if (append_typed_global_data(&candidate, object_type,
							initializers.front()))
							entry.data_items.swap(candidate.data_items);
						else
						{
							GlobalDefinition::DataItem zero;
							zero.kind = GlobalDefinition::DataItem::ITEM_ZERO;
							zero.zero_bytes = entry_type.storage_size();
							entry.data_items.push_back(zero);
							const PendingGlobalAction::Kind kind = is_thread_local &&
								model_.class_record_for_object_type(binding.type).valid() ?
								PendingGlobalAction::THREAD_LOCAL_CONSTRUCTION :
								PendingGlobalAction::AGGREGATE_VALUE;
							if (kind == PendingGlobalAction::THREAD_LOCAL_CONSTRUCTION)
								has_tls_construction = true;
							pending_global_actions_.push_back(PendingGlobalAction(
								kind, symbol, SymbolId(),
								Operand(), LowType(), binding.type, initializers.front(),
								source_declaration, source_declarator, binding_id));
						}
					}
				}
				else
				{
					entry.type = low_type(binding.type);
					if (entry.type.is_object())
					{
						// Emit zero storage only after the conservative typed checkpoint
						// check; do not synthesize lifetime or initializer actions.
						entry.structured = true;
						if (initializers.empty())
						{
							if (!checkpoint_zero_storage_eligible(binding.type))
								throw std::runtime_error(
									"PA15 namespace class zero storage is not proven trivial");
							GlobalDefinition::DataItem zero;
							zero.kind = GlobalDefinition::DataItem::ITEM_ZERO;
							zero.zero_bytes = entry.type.object_bytes;
							entry.data_items.push_back(zero);
							needs_trivial_namespace_object_init_ = true;
						}
						else
						{
							if (initializers.size() != 1)
								throw std::runtime_error(
									"PA15 global class initializer arity is invalid");
							GlobalDefinition candidate = entry;
							if (append_typed_global_data(&candidate, binding.type,
								initializers.front()))
								entry.data_items.swap(candidate.data_items);
							else
							{
								GlobalDefinition::DataItem zero;
								zero.kind = GlobalDefinition::DataItem::ITEM_ZERO;
								zero.zero_bytes = entry.type.object_bytes;
								entry.data_items.push_back(zero);
								const PendingGlobalAction::Kind kind = is_thread_local &&
									model_.class_record_for_object_type(binding.type).valid() ?
									PendingGlobalAction::THREAD_LOCAL_CONSTRUCTION :
									PendingGlobalAction::AGGREGATE_VALUE;
								if (kind == PendingGlobalAction::THREAD_LOCAL_CONSTRUCTION)
									has_tls_construction = true;
								pending_global_actions_.push_back(PendingGlobalAction(
									kind, symbol, SymbolId(),
									Operand(), LowType(), binding.type, initializers.front(),
									source_declaration, source_declarator, binding_id));
							}
						}
					}
					else if (initializers.empty())
						entry.init_kind = GlobalDefinition::INIT_ZERO;
					else if (entry.type.is_pointer() && typed_pointer_zero(
						initializers.front(), binding.type))
						entry.init_kind = GlobalDefinition::INIT_ZERO;
					else
					{
						const ConstantAddressFact* relocation = NULL;
						if (entry.type.is_pointer() && map_constant_address(
							initializers.front(), &entry.init_operand.symbol_id,
							&entry.addr_addend, &relocation))
						{
							if (relocation->kind == ConstantAddressKind::ArrayElement &&
								relocation->byte_addend != 0)
							{
								if (!relocation->index_fact.valid() ||
									!relocation->index_type.valid() ||
									!relocation->element_type.valid())
									throw std::runtime_error(
										"PA15 incomplete typed global projection");
								const LowType index_type =
									low_type(relocation->index_type);
								if (!index_type.is_integer())
									throw std::runtime_error(
										"PA15 noninteger global projection index");
								const Operand index = integer_operand(
									static_cast<long long>(relocation->index_value),
									index_type);
								pending_global_actions_.push_back(PendingGlobalAction(
									PendingGlobalAction::ADDRESS_PROJECTION, symbol,
									entry.init_operand.symbol_id, index,
									low_type(relocation->element_type), TypeId(),
									SemanticFactId(), source_declaration,
									source_declarator));
								entry.init_kind = GlobalDefinition::INIT_ZERO;
							}
							else
							{
								entry.init_kind = GlobalDefinition::INIT_ADDR;
								entry.init_operand.kind = Operand::OP_GLOBAL;
								if (entry.init_operand.symbol_id == symbol)
									throw std::runtime_error(
										"PA15 global initializer points at itself");
								entry.init_operand.presentation_id = symbol_name_for(
									entry.init_operand.symbol_id);
								if (!entry.init_operand.presentation_id.valid())
									throw std::runtime_error(
										"PA15 global initializer target has no name");
							}
						}
						else if (!constant_integer(initializers.front(), entry.type,
							&entry.init_operand))
						{
							entry.init_kind = GlobalDefinition::INIT_ZERO;
							pending_global_actions_.push_back(PendingGlobalAction(
								PendingGlobalAction::SCALAR_VALUE, symbol, SymbolId(),
								Operand(), LowType(), binding.type, initializers.front(),
								source_declaration, source_declarator));
						}
						else
							entry.init_kind = GlobalDefinition::INIT_INTEGER;
					}
				}
				program_.globals.push_back(entry);
				append_tls_wrapper(binding_id, ScopeId(scope_index), name_id);
				if (has_tls_construction)
					ensure_tls_lifetime_support(binding_id);
			}
		}
	}

} // namespace pa11_semantic_internal
