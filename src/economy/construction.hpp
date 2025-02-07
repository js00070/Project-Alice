#pragma once
#include "dcon_generated.hpp"

namespace sys {
struct state;
}

namespace economy {

void populate_construction_consumption(sys::state& state);

struct unit_construction_data {
	bool can_be_advanced;
	float construction_time;
	float cost_multiplier;
	dcon::nation_id owner;
	dcon::market_id market;
	dcon::province_id province;
	dcon::unit_type_id unit_type;
};
struct province_construction_spending_entry {
	dcon::province_building_construction_id construction;
	float spending;
};
struct state_construction_spending_entry {
	dcon::state_building_construction_id construction;
	float spending;
};
struct province_land_construction_spending_entry {
	dcon::province_land_construction_id construction;
	float spending;
};
struct province_naval_construction_spending_entry {
	dcon::province_naval_construction_id construction;
	float spending;
};
struct construction_spending_explanation {
	int32_t ongoing_projects;
	float budget_limit_per_project;
	float estimated_spendings;
	std::vector<province_construction_spending_entry> province_buildings;
	std::vector<state_construction_spending_entry> factories;
	std::vector<province_land_construction_spending_entry> land_units;
	std::vector<province_naval_construction_spending_entry> naval_units;
};
struct construction_spending_explanation_light {
	int32_t ongoing_projects;
	float budget_limit_per_project;
	float estimated_spendings;
	float province_buildings;
	float factories;
	float land_units;
	float naval_units;
};

construction_spending_explanation explain_construction_spending(
	sys::state& state,
	dcon::nation_id n,
	float dedicated_budget
);
void populate_private_construction_consumption(sys::state& state);
void advance_construction(sys::state& state, dcon::nation_id n, float total_spent_on_construction);
void emulate_construction_demand(sys::state& state, dcon::nation_id n);
construction_spending_explanation explain_construction_spending_now(sys::state& state, dcon::nation_id n);
economy::commodity_set calculate_factory_refit_goods_cost(sys::state& state, dcon::nation_id n, dcon::state_instance_id sid, dcon::factory_type_id from, dcon::factory_type_id to);
float calculate_factory_refit_money_cost(sys::state& state, dcon::nation_id n, dcon::state_instance_id sid, dcon::factory_type_id from, dcon::factory_type_id to);

float estimate_construction_spending_from_budget(sys::state& state, dcon::nation_id n, float current_budget);
float estimate_construction_spending(sys::state& state, dcon::nation_id n);
}
