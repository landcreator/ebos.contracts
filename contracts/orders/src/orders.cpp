#include <orders/orders.hpp>

namespace eosio{
	
 	void orders::upsert(uint128_t order_id, name account, std::string logistics, std::string goods_info, name merchant){

 		require_auth( get_self() );

 		order_index orders(get_self(), get_first_receiver().value);

 		auto order_id_index = orders.get_index<name("byorderid")>();	

 		auto iterator = order_id_index.find(order_id);

 		if( iterator == order_id_index.end() )
	    {
	    	orders.emplace(get_self(), [&]( auto& row ) {
	    		row.pkey = orders.available_primary_key();
	    		row.order_id = order_id;
        	row.account = account;
        	row.logistics = logistics;
        	row.goods_info = goods_info;
        	row.merchant = merchant;
        	row.timestamp = current_block_time();
	      });
	    }
	    else {
	      orders.modify(*iterator, get_self(), [&]( auto& row ) {
	        row.order_id = order_id;
	        row.account = account;
	        row.logistics = logistics;
	        row.goods_info = goods_info;
	        row.merchant = merchant;
	        row.timestamp = current_block_time();
	      });
	    }
 	}

 	void orders::erase(uint128_t order_id){

 		require_auth( get_self() );

 		order_index orders(get_self(), get_first_receiver().value);

 		auto order_id_index = orders.get_index<name("byorderid")>();	

 		auto iterator = order_id_index.find(order_id);

 		check(iterator != order_id_index.end(), "Order does not exist");

 		order_id_index.erase(iterator); 
 	}

};

