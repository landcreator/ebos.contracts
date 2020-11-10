/*  债务关系表
  字段    数据类型
  pkey      uint64_t  主索引，递增
  债务id   uint128_t 索引
  债务方   name
  债权方   name
  额度     asset
  手续费   asset
  profile   map<string string>
  时间戳   block_timestamp*/

 #include <eosio/eosio.hpp>
 #include <eosio/system.hpp>
 #include <eosio/asset.hpp>

using namespace eosio;

namespace eosio{
  class [[eosio::contract("debts")]] debts : public contract{
    public:
      using contract::contract;

      [[eosio::action]]
      void upsert(uint128_t debt_id, name debtor, name creditor, asset quantity, asset fee, std::map<std::string, std::string> profile);

      [[eosio::action]]
      void erase(uint128_t debt_id);

      using upsert_action = eosio::action_wrapper<"upsert"_n, &debts::upsert>;

      using erase_aciton = eosio::action_wrapper<"erase"_n, &debts::erase>;

    private:

      struct [[eosio::table]] debt{
        uint64_t pkey;
        uint128_t debt_id;
        name debtor;
        name creditor;
        asset quantity;
        asset fee;
        std::map<std::string, std::string> profile;
        block_timestamp timestamp;

        uint64_t primary_key() const { return pkey; }
        uint128_t get_secondary_1() const { return debt_id; }
      };

      using debt_index = eosio::multi_index<"debts"_n, debt, indexed_by<"bydebtid"_n, const_mem_fun<debt,
      uint128_t, &debt::get_secondary_1>>>;
  };
};