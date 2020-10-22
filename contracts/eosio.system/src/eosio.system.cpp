#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

#include <eosio/crypto.hpp>
#include <eosio/dispatcher.hpp>

#include <cmath>

namespace eosiosystem {

   using eosio::current_time_point;
   using eosio::token;

   double get_continuous_rate(int64_t annual_rate) {
      return std::log1p(double(annual_rate)/double(100*inflation_precision));
   }

   system_contract::system_contract( name s, name code, datastream<const char*> ds )
   :native(s,code,ds),
    _voters(get_self(), get_self().value),
    _producers(get_self(), get_self().value),
    _global(get_self(), get_self().value),
    _global2(get_self(), get_self().value),
    _global3(get_self(), get_self().value),
    _vwglobal(_self, _self.value),
    _acntype(_self, _self.value),
    _cwl(_self, _self.value)
   {
      //print( "construct system\n" );
      _gstate  = _global.exists() ? _global.get() : get_default_parameters();
      _gstate2 = _global2.exists() ? _global2.get() : eosio_global_state2{};
      _gstate3 = _global3.exists() ? _global3.get() : eosio_global_state3{};
      _vwstate = _vwglobal.exists() ? _vwglobal.get() : vote_weight_state{};
   }

   eosio_global_state system_contract::get_default_parameters() {
      eosio_global_state dp;
      get_blockchain_parameters(dp);
      return dp;
   }

   symbol system_contract::core_symbol()const {
      return _gstate2.core_symbol;
   }

   system_contract::~system_contract() {
      _global.set( _gstate, get_self() );
      _global2.set( _gstate2, get_self() );
      _global3.set( _gstate3, get_self() );
      _vwglobal.set( _vwstate, _self );
   }

   void system_contract::setparams( const eosio::blockchain_parameters& params ) {
      require_auth( get_self() );
      (eosio::blockchain_parameters&)(_gstate) = params;
      check( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   void system_contract::setgrtdcpu(uint32_t cpu)
   {
      require_auth(_self);
      const static uint32_t max_microsec = 60 * 1000 * 1000; // 60 seconds

      check( cpu <= max_microsec , "the value of cpu should not more then 60 seconds");
      check( cpu > _gstate2.guaranteed_cpu, "can not reduce cpu guarantee");
      _gstate2.guaranteed_cpu = cpu;

      // set_guaranteed_minimum_resources(0, cpu, 0);
   }

   void system_contract::setpriv( const name& account, uint8_t ispriv ) {
      require_auth( get_self() );
      set_privileged( account, ispriv );
   }

   void system_contract::setalimits( const name& account, int64_t cpu ) {
      require_auth( get_self() );

      user_resources_table userres( get_self(), account.value );
      auto ritr = userres.find( account.value );
      check( ritr == userres.end(), "only supports unlimited accounts" );

      set_resource_limits_cpu( account, cpu );
   }

   void system_contract::activate( const eosio::checksum256& feature_digest ) {
      require_auth( get_self() );
      preactivate_feature( feature_digest );
   }

   void system_contract::rmvproducer( const name& producer ) {
      require_auth( get_self() );
      auto prod = _producers.find( producer.value );
      check( prod != _producers.end(), "producer not found" );
      _producers.modify( prod, same_payer, [&](auto& p) {
            p.deactivate();
         });
   }

   void system_contract::updtrevision( uint8_t revision ) {
      require_auth( get_self() );
      check( _gstate2.revision < 255, "can not increment revision" ); // prevent wrap around
      check( revision == _gstate2.revision + 1, "can only increment revision by one" );
      check( revision <= 1, // set upper bound to greatest revision supported in the code
             "specified revision is not yet supported by the code" );
      _gstate2.revision = revision;
   }

   /**
    *  Called after a new account is created. This code enforces resource-limits rules
    *  for new accounts as well as new account naming conventions.
    *
    *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
    *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
    *  who can create accounts with the creator's name as a suffix.
    *
    */
   void system_contract::newaccount( const name&       creator,
                                     const name&       newact,
                                     ignore<authority> owner,
                                     ignore<authority> active ) {

      if( creator != get_self() ) {
         uint64_t tmp = newact.value >> 4;
         bool has_dot = false;

         for( uint32_t i = 0; i < 12; ++i ) {
           has_dot |= !(tmp & 0x1f);
           tmp >>= 5;
         }
         if( has_dot ) { // or is less than 12 characters
            auto suffix = newact.suffix();
            check( suffix != newact, "short root name must created by eosio authority" );
            check( creator == suffix, "only suffix may create this account" );
         }

         check( _gstate2.account_creation_fee.amount > 0, "account_creation_fee must set first" );
         transfer_action_type action_data{ creator, saving_account, _gstate2.account_creation_fee, "new account creation fee" };
         eosio::action( permission_level{ creator, "active"_n }, token_account, "transfer"_n, action_data ).send();
      }

      user_resources_table  userres( get_self(), newact.value );

      userres.emplace( newact, [&]( auto& res ) {
        res.owner = newact;
        res.net_weight = asset( 0, system_contract::get_core_symbol() );
        res.cpu_weight = asset( 0, system_contract::get_core_symbol() );
      });

      set_resource_limits( newact, 0, 0, 0 );
   }

   void native::setabi( const name& acnt, const std::vector<char>& abi ) {
      eosio::multi_index< "abihash"_n, abi_hash >  table(get_self(), get_self().value);
      auto itr = table.find( acnt.value );
      if( itr == table.end() ) {
         table.emplace( acnt, [&]( auto& row ) {
            row.owner = acnt;
            row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
         });
      } else {
         table.modify( itr, same_payer, [&]( auto& row ) {
            row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
         });
      }
   }

   void system_contract::setcode( name account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code ){
      if ( account != "eosio"_n && account != "eosio.token"_n && account != "eosio.msig"_n ){
         auto itr = _cwl.find( account.value );
         check(itr != _cwl.end(), "account not exist in table cwl");
      }
   }

   void system_contract::init( unsigned_int version, const symbol& core ) {
      require_auth( get_self() );
      check( version.value == 0, "unsupported version for init action" );

      auto system_token_supply   = eosio::token::get_supply(token_account, core.code() );
      check( system_token_supply.symbol == core, "specified core symbol does not exist (precision mismatch)" );
      check( system_token_supply.amount > 0, "system token supply must be greater than 0" );

      _gstate2.core_symbol = core;
   }

   void system_contract::setvweight( uint32_t company_weight, uint32_t government_weight ){
      require_auth( _self );
      check( 100 <= company_weight && company_weight <= 1000, "company_weight range is [100,1000]" );
      check( 100 <= government_weight && government_weight <= 1000, "company_weight range is [100,1000]" );
      _vwstate.company_weight = company_weight;
      _vwstate.government_weight = government_weight;
   }

   void system_contract::setacntfee( asset account_creation_fee ){
      require_auth( _self );
      check( core_symbol() == account_creation_fee.symbol, "token symbol not match" );
      check( 0 < account_creation_fee.amount && account_creation_fee.amount <= 10 * std::pow(10,core_symbol().precision()), (string("fee range is {0, 10.0 ") + core_symbol().code().to_string() + "]" ).c_str() );
      _gstate2.account_creation_fee = account_creation_fee;
   }

   void system_contract::setacntype( name acnt, name type ){
      require_auth( admin_account );
      check( is_account( acnt ), "account not exist");

      auto itr = _acntype.find( acnt.value );
      check(itr == _acntype.end(), "account already set");

      check( type == name_company || type == name_government , "type value must be one of [company, government]");

      _acntype.emplace( _self, [&]( auto& r ) {
         r.account = acnt;
         r.type = type ;
      });
   }

   void system_contract::awlset( string action, name account ){
      check( has_auth(admin_account) || has_auth(_self), "must have auth of admin or eosio");
      check( action == "add" || action == "delete" ,"action must be one of [add, delete]");

      if (action == "add"  ){
         auto itr = _cwl.find( account.value );
         check(itr == _cwl.end(), "account already exist");
         _cwl.emplace( _self, [&]( auto& r ) {
              r.account = account;
         });
      }

      if (action == "delete"  ){
         auto itr = _cwl.find( account.value );
         check(itr != _cwl.end(), "account not exist");
         _cwl.erase( itr );
      }
   }
} /// eosio.system
