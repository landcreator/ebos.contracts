/*订单表
字段    数据类型
pkey      uint64_t  主索引，递增
订单号   uint64_t 索引
地址(account)     name
物流信息  string
商品信息  string
商户     name
时间戳   block_timestamp*/
#pragma once

#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

using namespace eosio;

namespace eosio {
	
	class [[eosio::contract("orders")]] orders : public contract{
    public:

      using contract::contract;

      [[eosio::action]]
      void upsert(uint128_t order_id, name account, std::string logistics, std::string goods_info, name merchant);

      [[eosio::action]]
      void erase(uint128_t order_id);


      using upsert_action = eosio::action_wrapper<"upsert"_n, &orders::upsert>;

      using erase_action = eosio::action_wrapper<"erase"_n, &orders::erase>;

    private:
      struct [[eosio::table]] order
      {
        uint64_t pkey;
        uint128_t order_id;
        name account;
        std::string logistics;
        std::string goods_info;
        name merchant;
        block_timestamp timestamp;

        uint64_t primary_key() const{ return pkey; }
        uint128_t get_secondary_1() const { return order_id; }
      };

      using order_index = eosio::multi_index<"orders"_n, order, indexed_by<"byorderid"_n, const_mem_fun<order,
      uint128_t, &order::get_secondary_1>>>;
  };
};


