#pragma once

#include <eosio/action.hpp>
#include <eosio/contract.hpp>
#include <eosio/crypto.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/ignore.hpp>
#include <eosio/print.hpp>
#include <eosio/privileged.hpp>
#include <eosio/producer_schedule.hpp>

// This header is needed until `is_feature_activiated` and `preactivate_feature` are added to `eosio.cdt`
#include <eosio/../../capi/eosio/crypto.h>

namespace eosio {
   namespace internal_use_do_not_use {
      extern "C" {
         __attribute__((eosio_wasm_import))
         bool is_feature_activated( const ::capi_checksum256* feature_digest );

         __attribute__((eosio_wasm_import))
         void preactivate_feature( const ::capi_checksum256* feature_digest );
      }
   }

   bool is_feature_activated( const eosio::checksum256& feature_digest );
   void preactivate_feature( const eosio::checksum256& feature_digest );
}

namespace eosiosystem {

   using eosio::checksum256;
   using eosio::ignore;
   using eosio::name;
   using eosio::permission_level;
   using eosio::public_key;

   /**
    * @addtogroup eosiosystem
    * @{
    */
   /**
    * A weighted permission.
    *
    * @details Defines a weighted permission, that is a permission which has a weight associated.
    * A permission is defined by an account name plus a permission name.
    */
   struct permission_level_weight {
      permission_level  permission;
      uint16_t          weight;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( permission_level_weight, (permission)(weight) )
   };

   /**
    * Weighted key.
    *
    * @details A weighted key is defined by a public key and an associated weight.
    */
   struct key_weight {
      eosio::public_key  key;
      uint16_t           weight;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( key_weight, (key)(weight) )
   };

   /**
    * Wait weight.
    *
    * @details A wait weight is defined by a number of seconds to wait for and a weight.
    */
   struct wait_weight {
      uint32_t           wait_sec;
      uint16_t           weight;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( wait_weight, (wait_sec)(weight) )
   };

   /**
    * Blockchain authority.
    *
    * @details An authority is defined by:
    * - a vector of key_weights (a key_weight is a public key plus a wieght),
    * - a vector of permission_level_weights, (a permission_level is an account name plus a permission name)
    * - a vector of wait_weights (a wait_weight is defined by a number of seconds to wait and a weight)
    * - a threshold value
    */
   struct authority {
      uint32_t                              threshold = 0;
      std::vector<key_weight>               keys;
      std::vector<permission_level_weight>  accounts;
      std::vector<wait_weight>              waits;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( authority, (threshold)(keys)(accounts)(waits) )
   };

   /**
    * Blockchain block header.
    *
    * @details A block header is defined by:
    * - a timestamp,
    * - the producer that created it,
    * - a confirmed flag default as zero,
    * - a link to previous block,
    * - a link to the transaction merkel root,
    * - a link to action root,
    * - a schedule version,
    * - and a producers' schedule.
    */
   struct block_header {
      uint32_t                                  timestamp;
      name                                      producer;
      uint16_t                                  confirmed = 0;
      checksum256                               previous;
      checksum256                               transaction_mroot;
      checksum256                               action_mroot;
      uint32_t                                  schedule_version = 0;
      std::optional<eosio::producer_schedule>   new_producers;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE(block_header, (timestamp)(producer)(confirmed)(previous)(transaction_mroot)(action_mroot)
                                     (schedule_version)(new_producers))
   };

   /**
    * abi_hash
    *
    * @details abi_hash is the structure underlying the abihash table and consists of:
    * - `owner`: the account owner of the contract's abi
    * - `hash`: is the sha256 hash of the abi/binary
    */
   struct [[eosio::table("abihash"), eosio::contract("eosio.system")]] abi_hash {
      name              owner;
      checksum256       hash;
      uint64_t primary_key()const { return owner.value; }

      EOSLIB_SERIALIZE( abi_hash, (owner)(hash) )
   };

   // Method parameters commented out to prevent generation of code that parses input data.
   /**
    * The EOSIO core native contract that governs authorization and contracts' abi.
    *
    * @details
    */
   class [[eosio::contract("eosio.system")]] native : public eosio::contract {
      public:

         using eosio::contract::contract;

//         [[eosio::action]]
//         void newaccount( const name&       creator,
//                          const name&       name,
//                          ignore<authority> owner,
//                          ignore<authority> active);

         [[eosio::action]]
         void updateauth( ignore<name>      account,
                          ignore<name>      permission,
                          ignore<name>      parent,
                          ignore<authority> auth ) {}

         [[eosio::action]]
         void deleteauth( ignore<name> account,
                          ignore<name> permission ) {}

         [[eosio::action]]
         void linkauth( ignore<name> account,
                        ignore<name> code,
                        ignore<name> type,
                        ignore<name> requirement  ) {}

         [[eosio::action]]
         void unlinkauth( ignore<name> account,
                          ignore<name> code,
                          ignore<name> type ) {}

         [[eosio::action]]
         void canceldelay( ignore<permission_level> canceling_auth, ignore<checksum256> trx_id ) {}

         [[eosio::action]]
         void onerror( ignore<uint128_t> sender_id, ignore<std::vector<char>> sent_trx );

         [[eosio::action]]
         void setabi( const name& account, const std::vector<char>& abi );

         // [[eosio::action]]
         // void setcode( const name& account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code ) {}


//         using newaccount_action = eosio::action_wrapper<"newaccount"_n, &native::newaccount>;
         using updateauth_action = eosio::action_wrapper<"updateauth"_n, &native::updateauth>;
         using deleteauth_action = eosio::action_wrapper<"deleteauth"_n, &native::deleteauth>;
         using linkauth_action = eosio::action_wrapper<"linkauth"_n, &native::linkauth>;
         using unlinkauth_action = eosio::action_wrapper<"unlinkauth"_n, &native::unlinkauth>;
         using canceldelay_action = eosio::action_wrapper<"canceldelay"_n, &native::canceldelay>;
         // using setcode_action = eosio::action_wrapper<"setcode"_n, &native::setcode>;
         using setabi_action = eosio::action_wrapper<"setabi"_n, &native::setabi>;
   };

}
