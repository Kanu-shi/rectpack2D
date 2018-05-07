#pragma once
#include <optional>
#include <vector>
#include <array>
#include <variant>

#include "insert_and_split.h"
#include "empty_space_tree.h"
#include "empty_space_allocators.h"

#include "best_bin_finder.h"

namespace rectpack2D {
	template <class tree_type>
	using output_rect_t = typename tree_type::output_rect_type;

	template <class F, class G>
	struct finder_input {
		const int max_bin_side;
		const int discard_step;
		F handle_successful_insertion;
		G handle_unsuccessful_insertion;
	};

	template <class F, class G>
	auto make_finder_input(
		const int max_bin_side,
		const int discard_step,
		F&& handle_successful_insertion,
		G&& handle_unsuccessful_insertion
	) {
		return finder_input<F, G> { 
			max_bin_side, discard_step, std::forward<F>(handle_successful_insertion), std::forward<G>(handle_unsuccessful_insertion)
		};
	};

	/*
		Finds the best packing for the rectangles,
		just in the order that they were passed.
	*/

	template <class root_node_type, class F, class G>
	rect_wh find_best_packing_dont_sort(
		std::vector<output_rect_t<root_node_type>>& subjects,
		const finder_input<F, G>& input
	) {
		return find_best_packing_impl<root_node_type, std::remove_reference_t<decltype(subjects)>>(
			[&subjects](auto callback) { callback(subjects); },
			input
		);
	}

	template <class root_node_type, class F, class G, class Comparator, class... Comparators>
	rect_wh find_best_packing(
		std::vector<output_rect_t<root_node_type>>& subjects,
		const finder_input<F, G>& input,
		Comparator comparator,
		Comparators... comparators
	) {
		using rect_type = output_rect_t<root_node_type>;
		using order_type = std::vector<rect_type*>;

		constexpr auto count_orders = 1 + sizeof...(Comparators);
		thread_local std::array<order_type, count_orders> orders;

		{
			/* order[0] will always exist since this overload requires at least one comparator */
			auto& initial_pointers = orders[0];
			initial_pointers.clear();

			for (auto& s : subjects) {
				initial_pointers.emplace_back(std::addressof(s));
			}

			for (std::size_t i = 1; i < count_orders; ++i) {
				orders[i] = initial_pointers;
			}
		}

		std::size_t f = 0;

		auto make_order = [&f](auto& predicate) {
			std::sort(orders[f].begin(), orders[f].end(), predicate);
			++f;
		};

		make_order(comparator);
		(make_order(comparators), ...);

		return find_best_packing_impl<root_node_type, order_type>(
			[](auto callback){ for (auto& o : orders) { callback(o); } },
			input
		);
	}


	template <class root_node_type, class F, class G>
	rect_wh find_best_packing(
		std::vector<output_rect_t<root_node_type>>& subjects,
		const finder_input<F, G>& input
	) {
		using rect_type = output_rect_t<root_node_type>;

		return find_best_packing<root_node_type>(
			subjects,
			input,
			[](const rect_type* const a, const rect_type* const b) {
				return a->area() > b->area();
			},
			[](const rect_type* const a, const rect_type* const b) {
				return a->perimeter() > b->perimeter();
			},
			[](const rect_type* const a, const rect_type* const b) {
				return std::max(a->w, a->h) > std::max(b->w, b->h);
			},
			[](const rect_type* const a, const rect_type* const b) {
				return a->w > b->w;
			},
			[](const rect_type* const a, const rect_type* const b) {
				return a->h > b->h;
			},
			[](const rect_type* const a, const rect_type* const b) {
				return a->get_wh().pathological_mult() > b->get_wh().pathological_mult();
			}
		);
	}
}