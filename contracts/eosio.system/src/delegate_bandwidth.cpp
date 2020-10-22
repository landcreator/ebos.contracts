#include <eosio/datastream.hpp>
#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/privileged.hpp>
#include <eosio/serialize.hpp>
#include <eosio/transaction.hpp>

#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiosystem {

   using eosio::asset;
   using eosio::const_mem_fun;
   using eosio::current_time_point;
   using eosio::indexed_by;
   using eosio::permission_level;
   using eosio::seconds;
   using eosio::time_point_sec;
   using eosio::token;

   void system_contract::buyrambytes( const name& payer, const name& receiver, uint32_t bytes ) {
      check( bytes == 0, "buyrambytes action's bytes must be zero ");
   }

   void system_contract::buyram( const name& payer, const name& receiver, const asset& quant )
   {
      check( quant.amount == 0, "buyram action's asset.amount must be zero ");
   }

   void system_contract::changebw( name from, const name& receiver, const asset& stake_cpu_delta, bool transfer )
   {
      require_auth( from );
      check( stake_cpu_delta.amount != 0, "should stake non-zero stake_cpu_delta.amount" );

      name source_stake_from = from;
      if ( transfer ) {
         from = receiver;
      }

      // update stake delegated from "from" to "receiver"
      {
         del_bandwidth_table     del_tbl( _self, from.value );
         auto itr = del_tbl.find( receiver.value );
         if( itr == del_tbl.end() ) {
            itr = del_tbl.emplace( from, [&]( auto& dbo ){
                  dbo.from          = from;
                  dbo.to            = receiver;
                  dbo.cpu_weight    = stake_cpu_delta;
            });
         } else {
            del_tbl.modify( itr, same_payer, [&]( auto& dbo ){
                  dbo.cpu_weight    += stake_cpu_delta;
            });
         }

         check( 0 <= itr->cpu_weight.amount, "insufficient staked cpu bandwidth" );
         if ( itr->is_empty() ) {
            del_tbl.erase( itr );
         }
      } // itr can be invalid, should go out of scope

      // update totals of "receiver"
      {
         user_resources_table   totals_tbl( _self, receiver.value );
         auto tot_itr = totals_tbl.find( receiver.value );
         if( tot_itr ==  totals_tbl.end() ) {
            tot_itr = totals_tbl.emplace( from, [&]( auto& tot ) {
                  tot.owner      = receiver;
                  tot.cpu_weight = stake_cpu_delta;
            });
         } else {
            totals_tbl.modify( tot_itr, from == receiver ? from : same_payer, [&]( auto& tot ) {
                  tot.cpu_weight += stake_cpu_delta;
            });
         }

         check( 0 <= tot_itr->cpu_weight.amount, "insufficient staked total cpu bandwidth" );
         set_resource_limits_cpu( receiver, tot_itr->cpu_weight.amount );
         if ( tot_itr->is_empty() ) {
            totals_tbl.erase( tot_itr );
         }
      } // tot_itr can be invalid, should go out of scope

      // create refund or update from existing refund
      if ( stake_account != source_stake_from ) { //for eosio both transfer and refund make no sense
         refunds_table refunds_tbl( _self, from.value );
         auto req = refunds_tbl.find( from.value );

         //create/update/delete refund
         auto cpu_balance = stake_cpu_delta;
         bool need_deferred_trx = false;

         // redundant assertion also at start of changebw to protect against misuse of changebw
         bool is_undelegating = cpu_balance.amount < 0;
         bool is_delegating_to_self = (!transfer && from == receiver);

         if( is_delegating_to_self || is_undelegating ) {
            if ( req != refunds_tbl.end() ) { //need to update refund
               refunds_tbl.modify( req, same_payer, [&]( refund_request& r ) {
                  if ( cpu_balance.amount < 0 ) {
                     r.request_time = current_time_point();
                  }

                  r.cpu_amount -= cpu_balance;

                  if ( r.cpu_amount.amount < 0 ){
                     r.cpu_amount.amount = 0;
                     cpu_balance = -r.cpu_amount;
                  } else {
                     cpu_balance.amount = 0;
                  }
               });

               check( 0 <= req->cpu_amount.amount, "negative cpu refund amount" ); //should never happen

               if ( req->is_empty() ) {
                  refunds_tbl.erase( req );
                  need_deferred_trx = false;
               } else {
                  need_deferred_trx = true;
               }
            } else if ( cpu_balance.amount < 0 ) { //need to create refund
               refunds_tbl.emplace( from, [&]( refund_request& r ) {
                  r.owner = from;
                  r.cpu_amount = -cpu_balance;
                  cpu_balance.amount = 0;
                  r.request_time = current_time_point();
               });
               need_deferred_trx = true;
            } // else stake increase requested with no existing row in refunds_tbl -> nothing to do with refunds_tbl
         } /// end if is_delegating_to_self || is_undelegating

         if ( need_deferred_trx ) {
            eosio::transaction out;
            out.actions.emplace_back( permission_level{from, active_permission},  _self, "refund"_n,  from );
            out.delay_sec = refund_delay_sec;
            eosio::cancel_deferred( from.value );
            out.send( from.value, from, true );
         } else {
            eosio::cancel_deferred( from.value );
         }

         auto transfer_amount = cpu_balance;
         if ( 0 < transfer_amount.amount ) {
            token::transfer_action transfer_act{ token_account, { {source_stake_from, active_permission} } };
            transfer_act.send( source_stake_from, stake_account, asset(transfer_amount), "stake bandwidth" );
         }
      }

      update_voting_power( from, stake_cpu_delta );
   }

   void system_contract::update_voting_power( const name& voter, const asset& total_update )
   {
      int64_t old_staked = 0;
      int64_t new_staked = 0;

      auto voter_itr = _voters.find( voter.value );
      if( voter_itr == _voters.end() ) {
         voter_itr = _voters.emplace( voter, [&]( auto& v ) {
            v.owner  = voter;
            v.staked = total_update.amount;
         });
         old_staked = 0;
         new_staked = voter_itr->staked;
      } else {
         old_staked = voter_itr->staked;
         _voters.modify( voter_itr, same_payer, [&]( auto& v ) {
            v.staked += total_update.amount;
         });
         new_staked = voter_itr->staked;
      }

      check( 0 <= voter_itr->staked, "stake for voting cannot be negative" );

      if( voter_itr->producers.size() ) {
          auto itr = _acntype.find( voter.value );
          if( itr != _acntype.end() ){
             update_producers_votes( itr->type , false, voter_itr->producers, old_staked, voter_itr->producers, new_staked);
          }
      }
   }

   void system_contract::delegatebw( const name& from, const name& receiver,
                                     const asset& stake_net_quantity,
                                     const asset& stake_cpu_quantity, bool transfer ) {
      asset zero_asset( 0, core_symbol() );
      check( stake_net_quantity == zero_asset, "stake_net_quantity must be zero asset" );
      check( stake_cpu_quantity >  zero_asset, "must stake a positive amount" );
      check( stake_cpu_quantity.amount > 0, "must stake a positive amount" );
      check( !transfer || from != receiver, "cannot use transfer flag if delegating to self" );

      changebw( from, receiver, stake_cpu_quantity, transfer );
   }

   void system_contract::dlgtcpu( name from, name receiver, asset stake_cpu_quantity, bool transfer ){
      asset zero_asset( 0, core_symbol() );
      delegatebw( from, receiver, zero_asset, stake_cpu_quantity, transfer );
   }

   void system_contract::undelegatebw( const name& from, const name& receiver,
                                       const asset& unstake_net_quantity, const asset& unstake_cpu_quantity ){
      asset zero_asset( 0, core_symbol() );
      check( unstake_net_quantity == zero_asset, "unstake_net_quantity must be zero asset" );
      check( unstake_cpu_quantity >  zero_asset, "must unstake a positive amount" );
      check( unstake_cpu_quantity.amount + unstake_net_quantity.amount > 0, "must unstake a positive amount" );

      changebw( from, receiver, -unstake_cpu_quantity, false);
   } // undelegatebw

   void system_contract::undlgtcpu( name from, name receiver, asset unstake_cpu_quantity ){
      asset zero_asset( 0, core_symbol() );
      undelegatebw( from, receiver, zero_asset, unstake_cpu_quantity );
   }

   void system_contract::refund( const name& owner ) {
      require_auth( owner );

      refunds_table refunds_tbl( get_self(), owner.value );
      auto req = refunds_tbl.find( owner.value );
      check( req != refunds_tbl.end(), "refund request not found" );
      check( req->request_time + seconds(refund_delay_sec) <= current_time_point(), "refund is not available yet" );
      token::transfer_action transfer_act{ token_account, { {stake_account, active_permission}, {req->owner, active_permission} } };
      transfer_act.send( stake_account, req->owner, req->cpu_amount, "unstake" );
      refunds_tbl.erase( req );
   }


} //namespace eosiosystem
