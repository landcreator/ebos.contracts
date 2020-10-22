#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <eosio.system/native.hpp>

#include <deque>
#include <optional>
#include <string>
#include <type_traits>

namespace eosiosystem {

   using eosio::asset;
   using eosio::block_timestamp;
   using eosio::check;
   using eosio::const_mem_fun;
   using eosio::datastream;
   using eosio::indexed_by;
   using eosio::name;
   using eosio::same_payer;
   using eosio::symbol;
   using eosio::symbol_code;
   using eosio::time_point;
   using eosio::time_point_sec;
   using eosio::microseconds;
   using eosio::unsigned_int;
   using std::string;

   const static name name_company = "company"_n;
   const static name name_government = "government"_n;

   struct transfer_action_type {
      name    from;
      name    to;
      asset   quantity;
      string  memo;

      EOSLIB_SERIALIZE( transfer_action_type, (from)(to)(quantity)(memo) )
   };

   static void set_resource_limits_cpu( name account_value, int64_t cpu_weight ) {
      eosio::set_resource_limits( account_value, -1, -1, cpu_weight );
   }

   template<typename E, typename F>
   static inline auto has_field( F flags, E field )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, bool>
   {
      return ( (flags & static_cast<F>(field)) != 0 );
   }

   template<typename E, typename F>
   static inline auto set_field( F flags, E field, bool value = true )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, F >
   {
      if( value )
         return ( flags | static_cast<F>(field) );
      else
         return ( flags & ~static_cast<F>(field) );
   }



   static constexpr uint32_t seconds_per_year      = 52 * 7 * 24 * 3600;
   static constexpr uint32_t seconds_per_day       = 24 * 3600;
   static constexpr int64_t  useconds_per_year     = int64_t(seconds_per_year) * 1000'000ll;
   static constexpr int64_t  useconds_per_day      = int64_t(seconds_per_day) * 1000'000ll;
   static constexpr uint32_t blocks_per_day        = 2 * seconds_per_day; // half seconds per day

   static constexpr int64_t  min_activated_stake   = 150'000'000'0000;
   static constexpr int64_t  ram_gift_bytes        = 1400;
   static constexpr int64_t  min_pervote_daily_pay = 100'0000;
   static constexpr uint32_t refund_delay_sec      = 3 * seconds_per_day;

   static constexpr int64_t  inflation_precision           = 100;     // 2 decimals
   static constexpr int64_t  default_annual_rate           = 500;     // 5% annual rate
   static constexpr int64_t  pay_factor_precision          = 10000;
   static constexpr int64_t  default_inflation_pay_factor  = 50000;   // producers pay share = 10000 / 50000 = 20% of the inflation
   static constexpr int64_t  default_votepay_factor        = 40000;   // per-block pay share = 10000 / 40000 = 25% of the producer pay


   struct [[eosio::table("voteweight"), eosio::contract("eosio.system")]] vote_weight_state {
      vote_weight_state() {}
      uint32_t  company_weight = 100;        /// base number is 100
      uint32_t  government_weight = 100;     /// base number is 100

      EOSLIB_SERIALIZE( vote_weight_state, (company_weight)(government_weight) )
   };
   typedef eosio::singleton< "voteweight"_n, vote_weight_state >   vote_weight_singleton;

   struct [[eosio::table("acntype"), eosio::contract("eosio.system")]] ebos_account_type {
      ebos_account_type() { }
      name   account;
      name   type;  /// must be "company" or "government"

      uint64_t primary_key()const { return account.value; }
      EOSLIB_SERIALIZE( ebos_account_type, (account)(type) )
   };
   typedef eosio::multi_index< "acntype"_n, ebos_account_type >  account_type_table;

   struct [[eosio::table("cwl"), eosio::contract("eosio.system")]] ebos_contract_white_list {
      ebos_contract_white_list() { }
      name   account;

      uint64_t primary_key()const { return account.value; }
      EOSLIB_SERIALIZE( ebos_contract_white_list, (account) )
   };
   typedef eosio::multi_index< "cwl"_n, ebos_contract_white_list >  cwl_table;


   /**
    * eosio.system contract defines the structures and actions needed for blockchain's core functionality.
    * - Users can stake tokens for CPU and Network bandwidth, and then vote for producers or
    *    delegate their vote to a proxy.
    * - Producers register in order to be voted for, and can claim per-block and per-vote rewards.
    * - Users can buy and sell RAM at a market-determined price.
    * - Users can bid on premium names.
    * - A resource exchange system (REX) allows token holders to lend their tokens,
    *    and users to rent CPU and Network resources in return for a market-determined fee.
    */

   // Defines new global state parameters.
   struct [[eosio::table("global"), eosio::contract("eosio.system")]] eosio_global_state : eosio::blockchain_parameters {
      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

      uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
      uint64_t             total_ram_bytes_reserved = 0;
      int64_t              total_ram_stake = 0;

      block_timestamp      last_producer_schedule_update;
      time_point           last_pervote_bucket_fill;
      int64_t              pervote_bucket = 0;
      int64_t              perblock_bucket = 0;
      uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
      int64_t              total_activated_stake = 0;
      time_point           thresh_activated_stake_time;
      uint16_t             last_producer_schedule_size = 0;
      double               total_producer_vote_weight = 0; /// the sum of all producer votes
      block_timestamp      last_name_close;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE_DERIVED( eosio_global_state, eosio::blockchain_parameters,
                                (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                                (last_producer_schedule_update)(last_pervote_bucket_fill)
                                (pervote_bucket)(perblock_bucket)(total_unpaid_blocks)(total_activated_stake)(thresh_activated_stake_time)
                                (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close) )
   };

   // Defines new global state parameters added after version 1.0
   struct [[eosio::table("global2"), eosio::contract("eosio.system")]] eosio_global_state2 {
      eosio_global_state2(){}

      symbol            core_symbol;
      asset             account_creation_fee;
      uint32_t          guaranteed_cpu = 3 * 1000 * 1000; // 3 seconds
      uint8_t           revision = 0; ///< used to track version updates in the future.

      EOSLIB_SERIALIZE( eosio_global_state2, (core_symbol)(account_creation_fee)(guaranteed_cpu)(revision) )
   };

   // Defines new global state parameters added after version 1.3.0
   struct [[eosio::table("global3"), eosio::contract("eosio.system")]] eosio_global_state3 {
      eosio_global_state3() { }
      time_point        last_vpay_state_update;
      double            total_vpay_share_change_rate = 0;

      EOSLIB_SERIALIZE( eosio_global_state3, (last_vpay_state_update)(total_vpay_share_change_rate) )
   };


   // Defines `producer_info` structure to be stored in `producer_info` table, added after version 1.0
   struct [[eosio::table, eosio::contract("eosio.system")]] producer_info {
      name                  owner;
      double                total_vote_weight = 0;
      int64_t               company_votes = 0;
      int64_t               government_votes = 0;
      eosio::public_key     producer_key; /// a packed public key object
      bool                  is_active = true;
      std::string           url;
      uint32_t              unpaid_blocks = 0;
      time_point            last_claim_time;
      uint16_t              location = 0;
      uint64_t primary_key()const { return owner.value;                             }
      double   by_votes()const    { return is_active ? -total_vote_weight : total_vote_weight;  }
      bool     active()const      { return is_active;                               }
      void     deactivate()       { producer_key = public_key(); is_active = false; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( producer_info, (owner)(total_vote_weight)(company_votes)(government_votes)(producer_key)(is_active)(url)
                        (unpaid_blocks)(last_claim_time)(location) )
   };


   // Voter info. Voter info stores information about the voter:
   // - `owner` the voter
   // - `proxy` the proxy set by the voter, if any
   // - `producers` the producers approved by this voter if no proxy set
   // - `staked` the amount staked
   struct [[eosio::table, eosio::contract("eosio.system")]] voter_info {
      name                owner;     /// the voter
      name                proxy;     /// the proxy set by the voter, if any
      std::vector<name>   producers; /// the producers approved by this voter if no proxy set
      int64_t             staked = 0;

      //  Every time a vote is cast we must first "undo" the last vote weight, before casting the
      //  new vote weight.  Vote weight is calculated as:
      //  stated.amount * 2 ^ ( weeks_since_launch/weeks_per_year)
      double              last_vote_weight = 0; /// the vote weight cast the last time the vote was updated

      // Total vote weight delegated to this voter.
      double              proxied_vote_weight= 0; /// the total vote weight delegated to this voter as a proxy
      bool                is_proxy = 0; /// whether the voter is a proxy for others


      uint32_t            flags1 = 0;
      uint32_t            reserved2 = 0;
      eosio::asset        reserved3;

      uint64_t primary_key()const { return owner.value; }

      enum class flags1_fields : uint32_t {
         ram_managed = 1,
         net_managed = 2,
         cpu_managed = 4
      };

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( voter_info, (owner)(proxy)(producers)(staked)(last_vote_weight)(proxied_vote_weight)(is_proxy)(flags1)(reserved2)(reserved3) )
   };


   typedef eosio::multi_index< "voters"_n, voter_info >  voters_table;


   typedef eosio::multi_index< "producers"_n, producer_info,
                               indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
                             > producers_table;

   typedef eosio::singleton< "global"_n, eosio_global_state >   global_state_singleton;

   typedef eosio::singleton< "global2"_n, eosio_global_state2 > global_state2_singleton;

   typedef eosio::singleton< "global3"_n, eosio_global_state3 > global_state3_singleton;

   struct [[eosio::table, eosio::contract("eosio.system")]] user_resources {
      name          owner;
      asset         net_weight;
      asset         cpu_weight;
      int64_t       ram_bytes = 0;

      bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0 && ram_bytes == 0; }
      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes) )
   };

   // Every user 'from' has a scope/table that uses every receipient 'to' as the primary key.
   struct [[eosio::table, eosio::contract("eosio.system")]] delegated_bandwidth {
      name          from;
      name          to;
      asset         net_weight;
      asset         cpu_weight;

      bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0; }
      uint64_t  primary_key()const { return to.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight) )

   };

   struct [[eosio::table, eosio::contract("eosio.system")]] refund_request {
      name            owner;
      time_point_sec  request_time;
      eosio::asset    net_amount;
      eosio::asset    cpu_amount;

      bool is_empty()const { return net_amount.amount == 0 && cpu_amount.amount == 0; }
      uint64_t  primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( refund_request, (owner)(request_time)(net_amount)(cpu_amount) )
   };

   typedef eosio::multi_index< "userres"_n, user_resources >      user_resources_table;
   typedef eosio::multi_index< "delband"_n, delegated_bandwidth > del_bandwidth_table;
   typedef eosio::multi_index< "refunds"_n, refund_request >      refunds_table;


   /**
    * The EOSIO system contract. The EOSIO system contract governs ram market, voters, producers, global state.
    */
   class [[eosio::contract("eosio.system")]] system_contract : public native {

      private:
         voters_table            _voters;
         producers_table         _producers;
         global_state_singleton  _global;
         global_state2_singleton _global2;
         global_state3_singleton _global3;
         eosio_global_state      _gstate;
         eosio_global_state2     _gstate2;
         eosio_global_state3     _gstate3;
         vote_weight_singleton   _vwglobal;
         vote_weight_state       _vwstate;
         account_type_table      _acntype;
         cwl_table               _cwl;

      public:
         static constexpr eosio::name active_permission{"active"_n};
         static constexpr eosio::name token_account{"eosio.token"_n};
         static constexpr eosio::name stake_account{"eosio.stake"_n};
         static constexpr eosio::name saving_account{"eosio.saving"_n};
         static constexpr eosio::name admin_account{"dyadmin"_n};

         system_contract( name s, name code, datastream<const char*> ds );
         ~system_contract();

         static symbol get_core_symbol() {
            auto _global2 = global_state2_singleton("eosio"_n,"eosio"_n.value);
            check( _global2.exists(), "system contract not initialized");
            return _global2.get().core_symbol;
         }

         [[eosio::action]]
         void init( unsigned_int version, const symbol& core );

         [[eosio::action]]
         void onblock( ignore<block_header> header );

         [[eosio::action]]
         void setalimits( const name& account, int64_t cpu_weight );

         [[eosio::action]]
         void delegatebw( const name& from, const name& receiver,
                          const asset& stake_net_quantity, const asset& stake_cpu_quantity, bool transfer );

         [[eosio::action]]
         void dlgtcpu( name from, name receiver, asset stake_cpu_quantity, bool transfer );

         [[eosio::action]]
         void undelegatebw( const name& from, const name& receiver,
                            const asset& unstake_net_quantity, const asset& unstake_cpu_quantity );

         [[eosio::action]]
         void undlgtcpu( name from, name receiver, asset unstake_cpu_quantity );

         [[eosio::action]]
         void buyram( const name& payer, const name& receiver, const asset& quant );

         [[eosio::action]]
         void buyrambytes( const name& payer, const name& receiver, uint32_t bytes );

         [[eosio::action]]
         void refund( const name& owner );

         [[eosio::action]]
         void regproducer( const name producer, const public_key& producer_key, const std::string& url, uint16_t location );

         [[eosio::action]]
         void unregprod( const name producer );

         [[eosio::action]]
         void voteproducer( const name& voter, const name& proxy, const std::vector<name>& producers );

         [[eosio::action]]
         void setparams( const eosio::blockchain_parameters& params );

         [[eosio::action]]
         void setgrtdcpu( uint32_t cpu );

         [[eosio::action]]
         void setpriv( const name& account, uint8_t is_priv );

         [[eosio::action]]
         void setvweight( uint32_t company_weight, uint32_t government_weight );

         [[eosio::action]]
         void setacntfee( asset account_creation_fee );

         [[eosio::action]]
         void awlset( string action, name account );

         [[eosio::action]]
         void setcode( name account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code );

         [[eosio::action]]
         void rmvproducer( const name& producer );

         [[eosio::action]]
         void updtrevision( uint8_t revision );

         [[eosio::action]]
         void activate( const eosio::checksum256& feature_digest );

         [[eosio::action]]
         void setacntype( name account, name type );

         [[eosio::action]]
         void newaccount( const name&       creator,
                          const name&       newact,
                          ignore<authority> owner,
                          ignore<authority> active );

         [[eosio::action]]
         void claimrewards( const name& owner );



         using init_action = eosio::action_wrapper<"init"_n, &system_contract::init>;
         using activate_action = eosio::action_wrapper<"activate"_n, &system_contract::activate>;
         using delegatebw_action = eosio::action_wrapper<"delegatebw"_n, &system_contract::delegatebw>;
         using dlgtcpu_action = eosio::action_wrapper<"dlgtcpu"_n, &system_contract::dlgtcpu>;
         using undlgtcpu_action = eosio::action_wrapper<"undlgtcpu"_n, &system_contract::undlgtcpu>;
         using undelegatebw_action = eosio::action_wrapper<"undelegatebw"_n, &system_contract::undelegatebw>;
         using buyram_action = eosio::action_wrapper<"buyram"_n, &system_contract::buyram>;
         using buyrambytes_action = eosio::action_wrapper<"buyrambytes"_n, &system_contract::buyrambytes>;
         using refund_action = eosio::action_wrapper<"refund"_n, &system_contract::refund>;
         using regproducer_action = eosio::action_wrapper<"regproducer"_n, &system_contract::regproducer>;
         using unregprod_action = eosio::action_wrapper<"unregprod"_n, &system_contract::unregprod>;
         using voteproducer_action = eosio::action_wrapper<"voteproducer"_n, &system_contract::voteproducer>;
         using rmvproducer_action = eosio::action_wrapper<"rmvproducer"_n, &system_contract::rmvproducer>;
         using updtrevision_action = eosio::action_wrapper<"updtrevision"_n, &system_contract::updtrevision>;
         using setpriv_action = eosio::action_wrapper<"setpriv"_n, &system_contract::setpriv>;
         using setalimits_action = eosio::action_wrapper<"setalimits"_n, &system_contract::setalimits>;
         using setparams_action = eosio::action_wrapper<"setparams"_n, &system_contract::setparams>;
         using claimrewards_action = eosio::action_wrapper<"claimrewards"_n, &system_contract::claimrewards>;

         using setgrtdcpu_action = eosio::action_wrapper<"setgrtdcpu"_n, &system_contract::setgrtdcpu>;
         using setvweight_action = eosio::action_wrapper<"setvweight"_n, &system_contract::setvweight>;
         using setacntfee_action = eosio::action_wrapper<"setacntfee"_n, &system_contract::setacntfee>;
         using awlset_action = eosio::action_wrapper<"awlset"_n, &system_contract::awlset>;
         using setcode_action = eosio::action_wrapper<"setcode"_n, &system_contract::setcode>;
         using setacntype_action = eosio::action_wrapper<"setacntype"_n, &system_contract::setacntype>;
         using newaccount_action = eosio::action_wrapper<"newaccount"_n, &system_contract::newaccount>;

      private:
         //defined in eosio.system.cpp
         static eosio_global_state  get_default_parameters();
         symbol core_symbol()const;

         //defined in delegate_bandwidth.cpp
         void changebw( name from, const name& receiver,const asset& stake_cpu_quantity, bool transfer );
         void update_voting_power( const name& voter, const asset& total_update );

         //defined in voting.cpp
         void update_elected_producers( const block_timestamp& timestamp );
         void update_producers_votes( name type, bool voting, const std::vector<name>& old_producers, int64_t old_staked,
                                      const std::vector<name>& new_producers, int64_t new_staked );
   };

}
