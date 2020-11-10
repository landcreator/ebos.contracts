/*
 转账信息表
 字段     数据类型
 id       uint64_t  主索引，递增
 转账交易id   checksum256
 from        name
 to          name
 quantity    asset
 memo        string
 手续费   asset
 时间戳    block_timestamp*/

 #include <eosio/eosio.hpp>
 #include <eosio/system.hpp>
 #include <eosio/asset.hpp>

using namespace eosio;

namespace eosio{
  class [[eosio::contract("transrecords")]] transrecords : public contract{
    public:
      using contract::contract;

      [[eosio::action]]
      void upsert(checksum256 trans_id, name from, name to, asset quantity, std::string memo, asset fee);

      [[eosio::action]]
      void erase(checksum256 trans_id);

      using upsert_action = eosio::action_wrapper<"upsert"_n, &transrecords::upsert>;

      using erase_aciton = eosio::action_wrapper<"erase"_n, &transrecords::erase>;

    private:

      struct [[eosio::table]] transrecord{
        uint64_t pkey;
        checksum256 trans_id;
        name from;
        name to;
        asset quantity;
        std::string memo;
        asset fee;
        block_timestamp timestamp;

        uint64_t primary_key() const { return pkey; }
        checksum256 get_secondary_1() const { return trans_id; }
      };

      using transrecord_index = eosio::multi_index<"transrecords"_n, transrecord, indexed_by<"bytransid"_n, const_mem_fun<transrecord,
      checksum256, &transrecord::get_secondary_1>>>;
  };
};