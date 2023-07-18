#include "ai.hpp"
#include "system_state.hpp"

namespace ai {

void update_ai_general_status(sys::state& state) {
	for(auto n : state.world.in_nation) {
		if(state.world.nation_get_owned_province_count(n) == 0) {
			state.world.nation_set_ai_is_threatened(n, false);
			state.world.nation_set_ai_rival(n, dcon::nation_id{});
			continue;
		}

		auto ll = state.world.nation_get_last_war_loss(n);
		float safety_factor = 1.2f;
		if(ll && state.current_date < ll + 365 * 4) {
			safety_factor = 1.8f;
		}
		auto in_sphere_of = state.world.nation_get_in_sphere_of(n);

		float greatest_neighbor = 0.0f;
		for(auto b : state.world.nation_get_nation_adjacency_as_connected_nations(n)) {
			auto other = b.get_connected_nations(0) != n ? b.get_connected_nations(0) : b.get_connected_nations(1);
			if(!nations::are_allied(state, n, other) && (!in_sphere_of || in_sphere_of != other.get_in_sphere_of())) {
				greatest_neighbor = std::max(greatest_neighbor, float(state.world.nation_get_military_score(other)));
			}
		}

		float self_str = float(state.world.nation_get_military_score(n));
		for(auto subj : n.get_overlord_as_ruler()) {
			self_str += 0.75f * float(subj.get_subject().get_military_score());
		}

		float defensive_str = self_str;
		defensive_str += in_sphere_of ? float(state.world.nation_get_military_score(in_sphere_of)) : 0.0f;
		for(auto d : state.world.nation_get_diplomatic_relation(n)) {
			if(d.get_are_allied()) {
				auto other = d.get_related_nations(0) != n ? d.get_related_nations(0) : d.get_related_nations(1);
				defensive_str += float(state.world.nation_get_military_score(other));
			}
		}

		bool threatened = defensive_str < safety_factor * greatest_neighbor;
		state.world.nation_set_ai_is_threatened(n, threatened);

		if(!n.get_ai_rival()) {
			float min_relation = 200.0f;
			dcon::nation_id potential;
			for(auto adj : n.get_nation_adjacency()) {
				auto other = adj.get_connected_nations(0) != n ? adj.get_connected_nations(0) : adj.get_connected_nations(1);
				auto ol = other.get_overlord_as_subject().get_ruler();
				if(!ol && other.get_in_sphere_of() != n && (!threatened || !nations::are_allied(state, n, other))) {
					auto other_str = float(other.get_military_score());
					if(self_str * 0.5f < other_str && other_str <= self_str * 1.25) {
						auto rel = state.world.diplomatic_relation_get_value(state.world.get_diplomatic_relation_by_diplomatic_pair(n, other));
						if(rel < min_relation) {
							min_relation = rel;
							potential = other;
						}
					}
				}
			}

			if(potential) {
				if(!n.get_is_player_controlled() && nations::are_allied(state, n, potential)) {
					command::cancel_alliance(state, n, potential);
				}
				n.set_ai_rival(potential);
			}
		} else {
			auto rival_str = float(n.get_ai_rival().get_military_score());
			auto ol = n.get_ai_rival().get_overlord_as_subject().get_ruler();
			if(ol || n.get_ai_rival().get_in_sphere_of() == n || rival_str * 2 < self_str || self_str * 2 < rival_str) {
				n.set_ai_rival(dcon::nation_id{});
			}
		}
	}
}

void form_alliances(sys::state& state) {
	static std::vector<dcon::nation_id> alliance_targets;

	for(auto n : state.world.in_nation) {
		if(n.get_is_player_controlled() == false && n.get_ai_is_threatened() && !(n.get_overlord_as_subject().get_ruler())) {
			alliance_targets.clear();

			for(auto nb : n.get_nation_adjacency()) {
				auto other = nb.get_connected_nations(0) != n ? nb.get_connected_nations(0) : nb.get_connected_nations(1);
				if(other.get_is_player_controlled() == false && !(other.get_overlord_as_subject().get_ruler())  && !nations::are_allied(state, n, other) && !military::are_at_war(state, other, n) && ai_will_accept_alliance(state, other, n)) {
					alliance_targets.push_back(other.id);
				}
			}

			if(!alliance_targets.empty()) {
				std::sort(alliance_targets.begin(), alliance_targets.end(), [&](dcon::nation_id a, dcon::nation_id b) {
					if(state.world.nation_get_military_score(a) != state.world.nation_get_military_score(b))
						return state.world.nation_get_military_score(a) > state.world.nation_get_military_score(b);
					else
						return a.index() > b.index();
				});

				nations::make_alliance(state, n, alliance_targets[0]);
			}
		}
	}
}

bool ai_will_accept_alliance(sys::state& state, dcon::nation_id target, dcon::nation_id from) {
	if(!state.world.nation_get_ai_is_threatened(target))
		return false;

	if(state.world.nation_get_ai_rival(target) == from || state.world.nation_get_ai_rival(from) == target)
		return false;

	auto target_continent = state.world.province_get_continent(state.world.nation_get_capital(target));
	auto source_continent = state.world.province_get_continent(state.world.nation_get_capital(from));

	bool close_enough = (target_continent == source_continent) || bool(state.world.get_nation_adjacency_by_nation_adjacency_pair(target, from));

	if(!close_enough)
		return false;

	auto target_score = state.world.nation_get_military_score(target);
	auto source_score = state.world.nation_get_military_score(from);

	return source_score * 2 >= target_score;
}

void explain_ai_alliance_reasons(sys::state& state, dcon::nation_id target, text::layout_base& contents, int32_t indent) {

	text::add_line_with_condition(state, contents, "ai_alliance_1", state.world.nation_get_ai_is_threatened(target), indent);


	auto target_continent = state.world.province_get_continent(state.world.nation_get_capital(target));
	auto source_continent = state.world.province_get_continent(state.world.nation_get_capital(state.local_player_nation));

	bool close_enough = (target_continent == source_continent) || bool(state.world.get_nation_adjacency_by_nation_adjacency_pair(target, state.local_player_nation));

	text::add_line_with_condition(state, contents, "ai_alliance_2", close_enough, indent);

	text::add_line_with_condition(state, contents, "ai_alliance_3", state.world.nation_get_ai_rival(target) != state.local_player_nation && state.world.nation_get_ai_rival(state.local_player_nation) != target, indent);

	auto target_score = state.world.nation_get_military_score(target);
	auto source_score = state.world.nation_get_military_score(state.local_player_nation);

	text::add_line_with_condition(state, contents, "ai_alliance_4", source_score * 2 >= target_score, indent);
}

bool ai_will_grant_access(sys::state& state, dcon::nation_id target, dcon::nation_id from) {
	if(!state.world.nation_get_is_at_war(from))
		return false;
	if(state.world.nation_get_ai_rival(target) == from)
		return false;
	if(military::are_at_war(state, from, state.world.nation_get_ai_rival(target)))
		return true;

	for(auto wa : state.world.nation_get_war_participant(target)) {
		auto is_attacker = wa.get_is_attacker();
		for(auto o : wa.get_war().get_war_participant()) {
			if(o.get_is_attacker() != is_attacker) {
				if(military::are_at_war(state, o.get_nation(), from))
					return true;
			}
		}
	}
	return false;

}
void explain_ai_access_reasons(sys::state& state, dcon::nation_id target, text::layout_base& contents, int32_t indent) {
	text::add_line_with_condition(state, contents, "ai_access_1", ai_will_grant_access(state, target, state.local_player_nation), indent);
}

void update_ai_research(sys::state& state) {
	auto ymd_date = state.current_date.to_ymd(state.start_date);
	auto year = uint32_t(ymd_date.year);
	concurrency::parallel_for(uint32_t(0), state.world.nation_size(), [&](uint32_t id) {
		dcon::nation_id n{dcon::nation_id::value_base_t(id)};

		if(state.world.nation_get_is_player_controlled(n)
			|| state.world.nation_get_current_research(n)
			|| !state.world.nation_get_is_civilized(n)
			|| state.world.nation_get_owned_province_count(n) == 0) {

			//skip -- does not need new research
			return;
		}

		struct potential_techs {
			dcon::technology_id id;
			float weight = 0.0f;
		};

		std::vector<potential_techs> potential;

		for(auto tid : state.world.in_technology) {
			if(state.world.nation_get_active_technologies(n, tid))
				continue; // Already researched
		
			if(state.current_date.to_ymd(state.start_date).year >= state.world.technology_get_year(tid)) {
				// Find previous technology before this one
				dcon::technology_id prev_tech = dcon::technology_id(dcon::technology_id::value_base_t(tid.id.index() - 1));
				// Previous technology is from the same folder so we have to check that we have researched it beforehand
				if(tid.id.index() != 0 && state.world.technology_get_folder_index(prev_tech) == state.world.technology_get_folder_index(tid)) {
					// Only allow if all previously researched techs are researched
					if(state.world.nation_get_active_technologies(n, prev_tech))
						potential.push_back(potential_techs{tid, 0.0f});
				} else { // first tech in folder
					potential.push_back(potential_techs{ tid, 0.0f });
				}
			}
		}

		for(auto& pt : potential) { // weight techs
			auto base = state.world.technology_get_ai_weight(pt.id);
			if(state.world.nation_get_ai_is_threatened(n) && state.culture_definitions.tech_folders[state.world.technology_get_folder_index(pt.id)].category == culture::tech_category::army) {
				base *= 2.0f;
			}
			auto cost = std::max(1.0f, culture::effective_technology_cost(state, year, n, pt.id));
			pt.weight = base / cost;
		}
		auto rval = rng::get_random(state, id);
		std::sort(potential.begin(), potential.end(), [&](potential_techs& a, potential_techs& b) {
			if(a.weight != b.weight)
				return a.weight > b.weight;
			else // sort semi randomly
				return (a.id.index() ^ rval) > (b.id.index() ^ rval);
		});

		if(!potential.empty()) {
			state.world.nation_set_current_research(n, potential[0].id);
		}
	});
}

void initialize_ai_tech_weights(sys::state& state) {
	for(auto t : state.world.in_technology) {
		float base = 1000.0f;
		if(state.culture_definitions.tech_folders[t.get_folder_index()].category == culture::tech_category::army)
			base *= 1.5f;

		if(t.get_increase_naval_base())
			base *= 1.1f;
		else if(state.culture_definitions.tech_folders[t.get_folder_index()].category == culture::tech_category::navy)
			base *= 0.9f;

		auto mod = t.get_modifier();
		auto& vals = mod.get_national_values();
		for(uint32_t i = 0; i < sys::national_modifier_definition::modifier_definition_size; ++i) {
			if(vals.offsets[i] == sys::national_mod_offsets::research_points) {
				base *= 3.0f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::research_points_modifier) {
				base *= 3.0f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::education_efficiency) {
				base *= 2.0f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::education_efficiency_modifier) {
				base *= 2.0f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::pop_growth) {
				base *= 1.6f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::max_national_focus) {
				base *= 1.7f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::colonial_life_rating) {
				base *= 1.6f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::rgo_output) {
				base *= 1.2f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::factory_output) {
				base *= 1.2f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::factory_throughput) {
				base *= 1.2f;
			} else if(vals.offsets[i] == sys::national_mod_offsets::factory_input) {
				base *= 1.2f;
			}
		}

		t.set_ai_weight(base);
	}
}

void update_influence_priorities(sys::state& state) {
	struct weighted_nation {
		dcon::nation_id id;
		float weight = 0.0f;
	};
	static std::vector<weighted_nation> targets;

	for(auto gprl : state.world.in_gp_relationship) {
		if(gprl.get_great_power().get_is_player_controlled()) {
			// nothing -- player GP
		} else {
			auto& status = gprl.get_status();
			status &= ~nations::influence::priority_mask;
			if((status & nations::influence::level_mask) == nations::influence::level_in_sphere) {
				status |= nations::influence::priority_one;
			}
		}
	}

	for(auto&n : state.great_nations) {
		if(state.world.nation_get_is_player_controlled(n.nation))
			continue;

		targets.clear();
		for(auto t : state.world.in_nation) {
			if(t.get_is_great_power())
				continue;
			if(t.get_owned_province_count() == 0)
				continue;
			if(t.get_in_sphere_of() == n.nation)
				continue;
			if(t.get_demographics(demographics::total) > state.defines.large_population_limit)
				continue;

			float weight = 0.0f;

			for(auto c : state.world.in_commodity) {
				if(auto d = state.world.nation_get_real_demand(n.nation, c); d > 0.001f) {
					auto cweight = std::min(1.0f, t.get_domestic_market_pool(c) / d) * (1.0f - state.world.nation_get_demand_satisfaction(n.nation, c));
					weight += cweight;
				}
			}

			if(t.get_primary_culture().get_group_from_culture_group_membership() == state.world.nation_get_primary_culture(n.nation).get_group_from_culture_group_membership()) {
				weight += 4.0f;
			} else if(t.get_in_sphere_of()) {
				weight /= 3.0f;
			}

			if(state.world.get_nation_adjacency_by_nation_adjacency_pair(n.nation, t.id)) {
				weight *= 3.0f;
			}

			targets.push_back(weighted_nation{t.id, weight});
		}

		std::sort(targets.begin(), targets.end(), [](weighted_nation const& a, weighted_nation const& b) {
			if(a.weight != b.weight)
				return a.weight > b.weight;
			else
				return a.id.index() < b.id.index();
		});

		uint32_t i = 0;
		for(; i < 2 && i < targets.size(); ++i) {
			auto rel = state.world.get_gp_relationship_by_gp_influence_pair(targets[i].id, n.nation);
			if(!rel)
				rel = state.world.force_create_gp_relationship(targets[i].id, n.nation);
			state.world.gp_relationship_get_status(rel) |= nations::influence::priority_three;
		}
		for(; i < 4 && i < targets.size(); ++i) {
			auto rel = state.world.get_gp_relationship_by_gp_influence_pair(targets[i].id, n.nation);
			if(!rel)
				rel = state.world.force_create_gp_relationship(targets[i].id, n.nation);
			state.world.gp_relationship_get_status(rel) |= nations::influence::priority_two;
		}
		for(; i < 6 && i < targets.size(); ++i) {
			auto rel = state.world.get_gp_relationship_by_gp_influence_pair(targets[i].id, n.nation);
			if(!rel)
				rel = state.world.force_create_gp_relationship(targets[i].id, n.nation);
			state.world.gp_relationship_get_status(rel) |= nations::influence::priority_one;
		}
	}
}

void perform_influence_actions(sys::state& state) {
	for(auto gprl : state.world.in_gp_relationship) {
		if(gprl.get_great_power().get_is_player_controlled()) {
			// nothing -- player GP
		} else {
			if((gprl.get_status() & nations::influence::is_banned) != 0)
				continue; // can't do anything with a banned nation

			if(military::are_at_war(state, gprl.get_great_power(), gprl.get_influence_target()))
				continue; // can't do anything while at war

			auto clevel = (nations::influence::level_mask & gprl.get_status());
			if(clevel == nations::influence::level_in_sphere)
				continue; // already in sphere

			auto current_sphere = gprl.get_influence_target().get_in_sphere_of();

			if(state.defines.increaseopinion_influence_cost <= gprl.get_influence() && clevel != nations::influence::level_friendly) {

				gprl.get_influence() -= state.defines.increaseopinion_influence_cost;
				auto& l = gprl.get_status();
				l = nations::influence::increase_level(l);

				notification::post(state, notification::message{
					[source = gprl.get_great_power().id, influence_target = gprl.get_influence_target().id](sys::state& state, text::layout_base& contents) {
						text::add_line(state, contents, "msg_op_inc_1", text::variable_type::x, source, text::variable_type::y, influence_target);
					},
					"msg_op_inc_title",
					gprl.get_great_power().id,
					sys::message_setting_type::increase_opinion
				});
			} else if(state.defines.removefromsphere_influence_cost <= gprl.get_influence() && current_sphere /* && current_sphere != gprl.get_great_power()*/ && clevel == nations::influence::level_friendly) { // condition taken care of by check above

				gprl.get_influence() -= state.defines.removefromsphere_influence_cost;

				gprl.get_influence_target().set_in_sphere_of(dcon::nation_id{});

				auto orel = state.world.get_gp_relationship_by_gp_influence_pair(gprl.get_influence_target(), current_sphere);
				auto& l = state.world.gp_relationship_get_status(orel);
				l = nations::influence::decrease_level(l);

				nations::adjust_relationship(state, gprl.get_great_power(), current_sphere, state.defines.removefromsphere_relation_on_accept);
	
				notification::post(state, notification::message{
					[source = gprl.get_great_power().id, influence_target = gprl.get_influence_target().id, affected_gp = current_sphere.id](sys::state& state, text::layout_base& contents) {
						if(source == affected_gp)
							text::add_line(state, contents, "msg_rem_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target);
						else
							text::add_line(state, contents, "msg_rem_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target, text::variable_type::val, affected_gp);
					},
					"msg_rem_sphere_title",
					gprl.get_great_power(),
					sys::message_setting_type::rem_sphere_by_nation
				});
				notification::post(state, notification::message{
					[source = gprl.get_great_power().id, influence_target = gprl.get_influence_target().id, affected_gp = current_sphere.id](sys::state& state, text::layout_base& contents) {
						if(source == affected_gp)
							text::add_line(state, contents, "msg_rem_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target);
						else
							text::add_line(state, contents, "msg_rem_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target, text::variable_type::val, affected_gp);
					},
					"msg_rem_sphere_title",
					current_sphere,
					sys::message_setting_type::rem_sphere_on_nation
				});
				notification::post(state, notification::message{
					[source = gprl.get_great_power().id, influence_target = gprl.get_influence_target().id, affected_gp = current_sphere.id](sys::state& state, text::layout_base& contents) {
						if(source == affected_gp)
							text::add_line(state, contents, "msg_rem_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target);
						else
							text::add_line(state, contents, "msg_rem_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target, text::variable_type::val, affected_gp);
					},
					"msg_rem_sphere_title",
					gprl.get_influence_target(),
					sys::message_setting_type::removed_from_sphere
				});
			} else if(state.defines.addtosphere_influence_cost <= gprl.get_influence() && !current_sphere && clevel == nations::influence::level_friendly) {

				gprl.get_influence() -= state.defines.addtosphere_influence_cost;
				auto& l = gprl.get_status();
				l = nations::influence::increase_level(l);

				gprl.get_influence_target().set_in_sphere_of(gprl.get_great_power());

				notification::post(state, notification::message{
					[source = gprl.get_great_power().id, influence_target = gprl.get_influence_target().id](sys::state& state, text::layout_base& contents) {
						text::add_line(state, contents, "msg_add_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target);
					},
					"msg_add_sphere_title",
					gprl.get_great_power(),
					sys::message_setting_type::add_sphere
				});
				notification::post(state, notification::message{
					[source = gprl.get_great_power().id, influence_target = gprl.get_influence_target().id](sys::state& state, text::layout_base& contents) {
						text::add_line(state, contents, "msg_add_sphere_1", text::variable_type::x, source, text::variable_type::y, influence_target);
					},
					"msg_add_sphere_title",
					gprl.get_influence_target(),
					sys::message_setting_type::added_to_sphere
				});
			}
		}
	}
}

void identify_focuses(sys::state& state) {
	for(auto f : state.world.in_national_focus) {
		if(f.get_promotion_amount() > 0) {
			if(f.get_promotion_type() == state.culture_definitions.clergy)
				state.national_definitions.clergy_focus = f;
			if(f.get_promotion_type() == state.culture_definitions.soldiers)
				state.national_definitions.soldier_focus = f;
		}
	}
}

void update_focuses(sys::state& state) {
	for(auto si : state.world.in_state_instance) {
		if(!si.get_nation_from_state_ownership().get_is_player_controlled())
			si.set_owner_focus(dcon::national_focus_id{});
	}

	for(auto n : state.world.in_nation) {
		if(n.get_is_player_controlled())
			continue;
		if(n.get_owned_province_count() == 0)
			continue;

		n.set_state_from_flashpoint_focus(dcon::state_instance_id{});

		auto num_focuses_total = nations::max_national_focuses(state, n);
		if(num_focuses_total <= 0)
			return;

		auto base_opt = state.world.pop_type_get_research_optimum(state.culture_definitions.clergy);
		auto clergy_frac = n.get_demographics(demographics::to_key(state, state.culture_definitions.clergy)) / n.get_demographics(demographics::total);
		bool max_clergy = clergy_frac >= base_opt;

		static std::vector<dcon::state_instance_id> ordered_states;
		ordered_states.clear();
		for(auto si : n.get_state_ownership()) {
			ordered_states.push_back(si.get_state().id);
		}
		std::sort(ordered_states.begin(), ordered_states.end(), [&](auto a, auto b) {
			auto apop = state.world.state_instance_get_demographics(a, demographics::total);
			auto bpop = state.world.state_instance_get_demographics(b, demographics::total);
			if(apop != bpop)
				return apop > bpop;
			else
				return a.index() < b.index();
		});
		bool threatened = n.get_ai_is_threatened() || n.get_is_at_war();
		for(uint32_t i = 0; num_focuses_total > 0 && i < ordered_states.size(); ++i) {
			if(max_clergy) {
				if(threatened) {
					state.world.state_instance_set_owner_focus(ordered_states[i], state.national_definitions.soldier_focus);
					--num_focuses_total;
				} else {
					auto cfrac = state.world.state_instance_get_demographics(ordered_states[i], demographics::to_key(state, state.culture_definitions.clergy)) / state.world.state_instance_get_demographics(ordered_states[i], demographics::total);
					if(cfrac < state.defines.max_clergy_for_literacy * 0.8f) {
						state.world.state_instance_set_owner_focus(ordered_states[i], state.national_definitions.clergy_focus);
						--num_focuses_total;
					}
				}
			} else {
				auto cfrac = state.world.state_instance_get_demographics(ordered_states[i], demographics::to_key(state, state.culture_definitions.clergy)) / state.world.state_instance_get_demographics(ordered_states[i], demographics::total);
				if(cfrac < base_opt * 1.2f) {
					state.world.state_instance_set_owner_focus(ordered_states[i], state.national_definitions.clergy_focus);
					--num_focuses_total;
				}
			}
		}

	}
}

void take_ai_decisions(sys::state& state) {
	for(auto d : state.world.in_decision) {
		auto e = d.get_effect();
		if(!e)
			continue;

		auto potential = d.get_potential();
		auto allow = d.get_allow();
		auto ai_will_do = d.get_ai_will_do();

		ve::execute_serial_fast<dcon::nation_id>(state.world.nation_size(), [&](auto ids) {
			ve::vbitfield_type filter_a = potential
				? ve::compress_mask(trigger::evaluate(state, potential, trigger::to_generic(ids), trigger::to_generic(ids), 0)) & !state.world.nation_get_is_player_controlled(ids)
				: !state.world.nation_get_is_player_controlled(ids) ;

			if(filter_a.v != 0) {
				ve::mask_vector filter_c = allow
					? trigger::evaluate(state, allow, trigger::to_generic(ids), trigger::to_generic(ids), 0) && filter_a
					: ve::mask_vector{ filter_a };
				ve::mask_vector filter_b = ai_will_do
					? filter_c && (trigger::evaluate_multiplicative_modifier(state, ai_will_do, trigger::to_generic(ids), trigger::to_generic(ids), 0) > 0.0f)
					: filter_c;

				ve::apply([&](dcon::nation_id n, bool passed_filter) {
					if(passed_filter) {
						effect::execute(state, e, trigger::to_generic(n), trigger::to_generic(n), 0, uint32_t(state.current_date.value),
									uint32_t(n.index() << 4 ^ d.id.index()));

						notification::post(state, notification::message{
							[e, n, did = d.id, when = state.current_date](sys::state& state, text::layout_base& contents) {
								text::add_line(state, contents, "msg_decision_1", text::variable_type::x, n, text::variable_type::y, state.world.decision_get_name(did));
								text::add_line(state, contents, "msg_decision_2");
								ui::effect_description(state, contents, e, trigger::to_generic(n), trigger::to_generic(n), 0, uint32_t(when.value), uint32_t(n.index() << 4 ^ did.index()));
							},
							"msg_decision_title",
							n,
							sys::message_setting_type::decision
						});
					}
				}, ids, filter_b);
			}
		});
	}
}

void update_ai_econ_construction(sys::state& state) {
	for(auto n : state.world.in_nation) {
		// skip over: non ais, dead nations, and nations that aren't making money
		if(n.get_is_player_controlled() || n.get_owned_province_count() == 0 || n.get_spending_level() < 1.0f || n.get_last_treasury() >= n.get_stockpiles(economy::money))
			continue;

		auto treasury = n.get_stockpiles(economy::money);
		int32_t max_projects = std::max(2, int32_t(treasury / 8000.0f));

		auto rules = n.get_combined_issue_rules();
		auto current_iscore = n.get_industrial_score();
		if(current_iscore < 10) {
			if((rules & issue_rule::build_factory) == 0) { // try to jumpstart econ
				bool can_appoint = [&]() {

					if(!politics::can_appoint_ruling_party(state, n))
						return false;
					auto last_change = state.world.nation_get_ruling_party_last_appointed(n);
					if(last_change && state.current_date < last_change + 365)
						return false;
					if(politics::is_election_ongoing(state, n))
						return false;
					return true;
					/*auto gov = state.world.nation_get_government_type(source);
					auto new_ideology = state.world.political_party_get_ideology(p);
					if((state.culture_definitions.governments[gov].ideologies_allowed & ::culture::to_bits(new_ideology)) == 0) {
						return false;
					}*/
				}();

				if(can_appoint) {
					dcon::political_party_id target;

					auto gov = n.get_government_type();
					auto identity = n.get_identity_from_identity_holder();
					auto start = state.world.national_identity_get_political_party_first(identity).id.index();
					auto end = start + state.world.national_identity_get_political_party_count(identity);

					for(int32_t i = start; i < end && !target; i++) {
						auto pid = dcon::political_party_id(uint16_t(i));
						if(politics::political_party_is_active(state, pid) && (state.culture_definitions.governments[gov].ideologies_allowed & ::culture::to_bits(state.world.political_party_get_ideology(pid))) != 0) {

							for(auto pi : state.culture_definitions.party_issues) {
								auto issue_rules = state.world.political_party_get_party_issues(pid, pi).get_rules();
								if((issue_rules & issue_rule::build_factory) != 0) {
									target = pid;
									break;
								}
							}
						}
					}

					if(target) {
						politics::appoint_ruling_party(state, n, target);
						rules = n.get_combined_issue_rules();
					}
				} // END if(can_appoint)
			} // END if((rules & issue_rule::build_factory) == 0)
		} // END if(current_iscore < 10)


		if((rules & issue_rule::expand_factory) != 0 || (rules & issue_rule::build_factory) != 0) {
			static::std::vector<dcon::factory_type_id> desired_types;
			desired_types.clear();

			// first pass: try to fill shortages
			for(auto type : state.world.in_factory_type) {
				if(n.get_active_building(type) || type.get_is_available_from_start()) {
					bool lacking_output = n.get_demand_satisfaction(type.get_output()) < 1.0f;

					if(lacking_output) {
						auto& inputs = type.get_inputs();
						bool lacking_input = false;

						for(uint32_t i = 0; i < economy::commodity_set::set_size; ++i) {
							if(inputs.commodity_type[i]) {
								if(n.get_demand_satisfaction(inputs.commodity_type[i]) < 1.0f)
									lacking_input = true;
							} else {
								break;
							}
						}

						if(!lacking_input)
							desired_types.push_back(type.id);
					}
				} // END if building unlocked
			}

			if(desired_types.empty()) { // second pass: try to make money
				for(auto type : state.world.in_factory_type) {
					if(n.get_active_building(type) || type.get_is_available_from_start()) {
						auto& inputs = type.get_inputs();
						bool lacking_input = false;

						for(uint32_t i = 0; i < economy::commodity_set::set_size; ++i) {
							if(inputs.commodity_type[i]) {
								if(n.get_demand_satisfaction(inputs.commodity_type[i]) < 1.0f)
									lacking_input = true;
							} else {
								break;
							}
						}

						if(!lacking_input)
							desired_types.push_back(type.id);
					} // END if building unlocked
				}
			}

			// desired types filled: try to construct or upgrade
			if(!desired_types.empty()) {
				static std::vector<dcon::state_instance_id> ordered_states;
				ordered_states.clear();
				for(auto si : n.get_state_ownership()) {
					if(si.get_state().get_capital().get_is_colonial() == false)
						ordered_states.push_back(si.get_state().id);
				}
				std::sort(ordered_states.begin(), ordered_states.end(), [&](auto a, auto b) {
					auto apop = state.world.state_instance_get_demographics(a, demographics::total);
					auto bpop = state.world.state_instance_get_demographics(b, demographics::total);
					if(apop != bpop)
						return apop > bpop;
					else
						return a.index() < b.index();
				});

				if((rules & issue_rule::build_factory) == 0) { // can't build -- by elimination, can upgrade
					for(auto si : ordered_states) {
						if(max_projects <= 0)
							break;

						auto pw_num = state.world.state_instance_get_demographics(si,
								demographics::to_key(state, state.culture_definitions.primary_factory_worker));
						auto pw_employed = state.world.state_instance_get_demographics(si,
								demographics::to_employment_key(state, state.culture_definitions.primary_factory_worker));

						if(pw_employed >= pw_num && pw_num > 0.0f)
							continue; // no spare workers

						province::for_each_province_in_state_instance(state, si, [&](dcon::province_id p) {
							for(auto fac : state.world.province_get_factory_location(p)) {
								auto type = fac.get_factory().get_building_type();
								if(fac.get_factory().get_unprofitable() == false
									&& fac.get_factory().get_level() < uint8_t(255)
									&& std::find(desired_types.begin(), desired_types.end(), type) != desired_types.end()) {

									auto ug_in_progress = false;
									for(auto c : state.world.state_instance_get_state_building_construction(si)) {
										if(c.get_type() == type) {
											ug_in_progress = true;
											break;
										}
									}
									if(!ug_in_progress) {
										auto new_up = fatten(state.world, state.world.force_create_state_building_construction(si, n));
										new_up.set_is_pop_project(false);
										new_up.set_is_upgrade(true);
										new_up.set_type(type);

										--max_projects;
										return;
									}
								}
							}
						});
					} // END for(auto si : ordered_states) {
				} else { // if if((rules & issue_rule::build_factory) == 0) -- i.e. if building is possible
					for(auto si : ordered_states) {
						if(max_projects <= 0)
							break;

						// check -- either unemployed factory workers or no factory workers
						auto pw_num = state.world.state_instance_get_demographics(si,
								demographics::to_key(state, state.culture_definitions.primary_factory_worker));
						auto pw_employed = state.world.state_instance_get_demographics(si,
								demographics::to_employment_key(state, state.culture_definitions.primary_factory_worker));

						if(pw_employed >= pw_num && pw_num > 0.0f)
							continue; // no spare workers

						auto type_selection = desired_types[rng::get_random(state, uint32_t(n.id.index() + max_projects)) % desired_types.size()];
						assert(type_selection);

						if(state.world.factory_type_get_is_coastal(type_selection) && !province::state_is_coastal(state, si))
							continue;

						bool already_in_progress = [&]() {
							for(auto p : state.world.state_instance_get_state_building_construction(si)) {
								if(p.get_type() == type_selection)
									return true;
							}
							return false;
						}();
						if(already_in_progress)
							continue;

						if((rules & issue_rule::expand_factory) != 0) { // check: if present, try to upgrade
							bool present_in_location = false;
							province::for_each_province_in_state_instance(state, si, [&](dcon::province_id p) {
								for(auto fac : state.world.province_get_factory_location(p)) {
									auto type = fac.get_factory().get_building_type();
									if(type_selection == type) {
										present_in_location = true;
										return;
									}
								}
							});
							if(present_in_location) {
								auto new_up = fatten(state.world, state.world.force_create_state_building_construction(si, n));
								new_up.set_is_pop_project(false);
								new_up.set_is_upgrade(true);
								new_up.set_type(type_selection);

								--max_projects;
								continue;
							}
						}

						// else -- try to build -- must have room
						int32_t num_factories = 0;

						auto d = state.world.state_instance_get_definition(si);
						for(auto p : state.world.state_definition_get_abstract_state_membership(d)) {
							if(p.get_province().get_nation_from_province_ownership() == n) {
								for(auto f : p.get_province().get_factory_location()) {
									++num_factories;
								}
							}
						}
						for(auto p : state.world.state_instance_get_state_building_construction(si)) {
							if(p.get_is_upgrade() == false)
								++num_factories;
						}
						if(num_factories <= int32_t(state.defines.factories_per_state)) {
							auto new_up = fatten(state.world, state.world.force_create_state_building_construction(si, n));
							new_up.set_is_pop_project(false);
							new_up.set_is_upgrade(false);
							new_up.set_type(type_selection);

							--max_projects;
							continue;
						} else {
							// TODO: try to delete a factory here
						}

					} // END for(auto si : ordered_states) {
				} // END if((rules & issue_rule::build_factory) == 0) 
			} // END if(!desired_types.empty()) {
		} // END  if((rules & issue_rule::expand_factory) != 0 || (rules & issue_rule::build_factory) != 0)

		static std::vector<dcon::province_id> project_provs;

		// try naval bases
		if(max_projects > 0) {
			project_provs.clear();
			for(auto o : n.get_province_ownership()) {
				if(!o.get_province().get_is_coast())
					continue;
				if(n != o.get_province().get_nation_from_province_control())
					continue;
				if(military::province_is_under_siege(state, o.get_province()))
					continue;
				if(o.get_province().get_naval_base_level() == 0 && o.get_province().get_state_membership().get_naval_base_is_taken())
					continue;

				int32_t current_lvl = o.get_province().get_naval_base_level();
				int32_t max_local_lvl = n.get_max_naval_base_level();
				int32_t min_build = int32_t(o.get_province().get_modifier_values(sys::provincial_mod_offsets::min_build_naval_base));

				if(max_local_lvl - current_lvl - min_build <= 0)
					continue;

				if(!province::has_naval_base_being_built(state, o.get_province()))
					project_provs.push_back(o.get_province().id);
			}

			auto cap = n.get_capital();
			std::sort(project_provs.begin(), project_provs.end(), [&](dcon::province_id a, dcon::province_id b) {
				auto a_dist = province::direct_distance(state, a, cap);
				auto b_dist = province::direct_distance(state, b, cap);
				if(a_dist != b_dist)
					return a_dist < b_dist;
				else
					return a.index() < b.index();
			});
			if(!project_provs.empty()) {
				auto si = state.world.province_get_state_membership(project_provs[0]);
				if(si)
					si.set_naval_base_is_taken(true);
				auto new_rr = fatten(state.world, state.world.force_create_province_building_construction(project_provs[0], n));
				new_rr.set_is_pop_project(false);
				new_rr.set_type(uint8_t(economy::province_building_type::naval_base));
				--max_projects;
			}
		}

		// try railroads
		if((rules & issue_rule::build_railway) != 0 && max_projects > 0) {
			project_provs.clear();
			for(auto o : n.get_province_ownership()) {
				if(n != o.get_province().get_nation_from_province_control())
					continue;
				if(military::province_is_under_siege(state, o.get_province()))
					continue;

				int32_t current_rails_lvl = state.world.province_get_railroad_level(o.get_province());
				int32_t max_local_rails_lvl = state.world.nation_get_max_railroad_level(n);
				int32_t min_build_railroad =
					int32_t(state.world.province_get_modifier_values(o.get_province(), sys::provincial_mod_offsets::min_build_railroad));

				if(max_local_rails_lvl - current_rails_lvl - min_build_railroad <= 0)
					continue;

				if(!province::has_railroads_being_built(state, o.get_province())) {
					project_provs.push_back(o.get_province().id);
				}
			}

			auto cap = n.get_capital();
			std::sort(project_provs.begin(), project_provs.end(), [&](dcon::province_id a, dcon::province_id b) {
				auto a_dist = province::direct_distance(state, a, cap);
				auto b_dist = province::direct_distance(state, b, cap);
				if(a_dist != b_dist)
					return a_dist < b_dist;
				else
					return a.index() < b.index();
			});

			for(uint32_t i = 0; i < project_provs.size() && max_projects > 0; ++i) {
				auto new_rr = fatten(state.world, state.world.force_create_province_building_construction(project_provs[i], n));
				new_rr.set_is_pop_project(false);
				new_rr.set_type(uint8_t(economy::province_building_type::railroad));
				--max_projects;
			}
		}

		// try forts
		if(max_projects > 0) {
			project_provs.clear();

			for(auto o : n.get_province_ownership()) {
				if(n != o.get_province().get_nation_from_province_control())
					continue;
				if(military::province_is_under_siege(state, o.get_province()))
					continue;

				int32_t current_lvl = state.world.province_get_fort_level(o.get_province());
				int32_t max_local_lvl = state.world.nation_get_max_fort_level(n);
				int32_t min_build = int32_t(state.world.province_get_modifier_values(o.get_province(), sys::provincial_mod_offsets::min_build_fort));

				if(max_local_lvl - current_lvl - min_build <= 0)
					continue;

				if(!province::has_fort_being_built(state, o.get_province())) {
					project_provs.push_back(o.get_province().id);
				}
			}

			auto cap = n.get_capital();
			std::sort(project_provs.begin(), project_provs.end(), [&](dcon::province_id a, dcon::province_id b) {
				auto a_dist = province::direct_distance(state, a, cap);
				auto b_dist = province::direct_distance(state, b, cap);
				if(a_dist != b_dist)
					return a_dist < b_dist;
				else
					return a.index() < b.index();
			});

			for(uint32_t i = 0; i < project_provs.size() && max_projects > 0; ++i) {
				auto new_rr = fatten(state.world, state.world.force_create_province_building_construction(project_provs[i], n));
				new_rr.set_is_pop_project(false);
				new_rr.set_type(uint8_t(economy::province_building_type::fort));
				--max_projects;
			}
		}
	}
}

void update_ai_colonial_investment(sys::state& state) {
	static std::vector<dcon::state_definition_id> investments;
	static std::vector<int32_t> free_points;

	investments.clear();
	investments.resize(uint32_t(state.defines.colonial_rank));

	free_points.clear();
	free_points.resize(uint32_t(state.defines.colonial_rank), -1);

	for(auto col : state.world.in_colonization) {
		auto n = col.get_colonizer();
		if(n.get_is_player_controlled() == false
			&& n.get_rank() <= uint16_t(state.defines.colonial_rank)
			&& !investments[n.get_rank() - 1]
			&& col.get_state().get_colonization_stage() <= uint8_t(2)
			&& state.crisis_colony != col.get_state()
			&& (!state.crisis_war || n.get_is_at_war() == false)
			 ) {

			auto crange = col.get_state().get_colonization();
			if(crange.end() - crange.begin() > 1) {
				if(col.get_last_investment() + int32_t(state.defines.colonization_days_between_investment) <= state.current_date) {

					if(free_points[n.get_rank() - 1] < 0) {
						free_points[n.get_rank() - 1] = nations::free_colonial_points(state, n);
					}

					int32_t cost = 0;;
					if(col.get_state().get_colonization_stage() == 1) {
						cost = int32_t(state.defines.colonization_interest_cost);
					} else if(col.get_level() <= 4) {
						cost = int32_t(state.defines.colonization_influence_cost);
					} else {
						cost =
							int32_t(state.defines.colonization_extra_guard_cost * (col.get_level() - 4) + state.defines.colonization_influence_cost);
					}
					if(free_points[n.get_rank() - 1] >= cost) {
						investments[n.get_rank() - 1] = col.get_state().id;
					}
				}
			}
		}
	}
	for(uint32_t i = 0; i < investments.size(); ++i) {
		if(investments[i])
			province::increase_colonial_investment(state, state.nations_by_rank[i], investments[i]);
	}
}
void update_ai_colony_starting(sys::state& state) {
	static std::vector<int32_t> free_points;
	free_points.clear();
	free_points.resize(uint32_t(state.defines.colonial_rank), -1);
	for(int32_t i = 0; i < int32_t(state.defines.colonial_rank); ++i) {
		if(state.world.nation_get_is_player_controlled(state.nations_by_rank[i])) {
			free_points[i] = 0;
		} else {
			if(military::get_role(state, state.crisis_war, state.nations_by_rank[i]) != military::war_role::none) {
				free_points[i] = 0;
			} else {
				free_points[i] = nations::free_colonial_points(state, state.nations_by_rank[i]);
			}
		}
	}
	for(auto sd : state.world.in_state_definition) {
		if(sd.get_colonization_stage() <= 1) {
			bool has_unowned_land = false;
			bool state_is_coastal = false;

			for(auto p : state.world.state_definition_get_abstract_state_membership(sd)) {
				if(!p.get_province().get_nation_from_province_ownership()) {
					if(p.get_province().get_is_coast())
						state_is_coastal = true;
					if(p.get_province().id.index() < state.province_definitions.first_sea_province.index())
						has_unowned_land = true;
				}
			}
			if(has_unowned_land) {
				for(int32_t i = 0; i < int32_t(state.defines.colonial_rank); ++i) {
					if(free_points[i] > 0) {
						bool adjacent = false;
						if(province::fast_can_start_colony(state, state.nations_by_rank[i], sd, free_points[i], state_is_coastal, adjacent)) {
							free_points[i] -= int32_t(state.defines.colonization_interest_cost_initial + (adjacent ? state.defines.colonization_interest_cost_neighbor_modifier : 0.0f));

							auto new_rel = fatten(state.world, state.world.force_create_colonization(sd, state.nations_by_rank[i]));
							new_rel.set_level(uint8_t(1));
							new_rel.set_last_investment(state.current_date);
							new_rel.set_points_invested(uint16_t(state.defines.colonization_interest_cost_initial + (adjacent ? state.defines.colonization_interest_cost_neighbor_modifier : 0.0f)));

							state.world.state_definition_set_colonization_stage(sd, uint8_t(1));
						}
					}
				}
			}
		}
	}
}

void upgrade_colonies(sys::state& state) {
	for(auto si : state.world.in_state_instance) {
		if(si.get_capital().get_is_colonial() && si.get_nation_from_state_ownership().get_is_player_controlled() == false) {
			if(province::can_integrate_colony(state, si)) {
				province::upgrade_colonial_state(state, si.get_nation_from_state_ownership(), si);
			}
		}
	}
}

}