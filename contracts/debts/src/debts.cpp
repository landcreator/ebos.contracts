#include <debts/debts.hpp>

namespace eosio{
  void debts::upsert(uint128_t debt_id, name debtor, name creditor, asset quantity, asset fee, std::map<std::string, std::string> profile){
    require_auth(get_self());

    check( debtor != creditor, "debtor and creditor cannot be same one" );
    check( is_account( debtor ), "debtor account does not exist");
    check( is_account( creditor ), "creditor account does not exist");

    require_recipient( debtor );
    require_recipient( creditor );

    check( quantity.is_valid(), "invalid quantity" );
    check( fee.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( fee.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == fee.symbol, "symbol precision mismatch" );

    debt_index debts(get_self(), get_first_receiver().value);

    auto debt_id_index = debts.get_index<name("bydebtid")>();

    auto iterator = debt_id_index.find(debt_id);

    if( iterator == debt_id_index.end()){
      debts.emplace(get_self(), [&]( auto& row){
        row.pkey = debts.available_primary_key();
        row.debt_id = debt_id;
        row.debtor = debtor;
        row.creditor = creditor;
        row.quantity = quantity;
        row.fee = fee;
        row.profile.clear();
        row.profile = profile;
        row.timestamp = current_block_time();
      });
    }
    else{
      debts.modify(*iterator, get_self(), [&](auto& row){
        row.debt_id = debt_id;
        row.debtor = debtor;
        row.creditor = creditor;
        row.quantity = quantity;
        row.fee = fee;
        row.profile.clear();
        row.profile = profile;
        row.timestamp = current_block_time();
      });
    }
  }


  void debts::erase(uint128_t debt_id){
    require_auth(get_self());

    debt_index debts(get_self(), get_first_receiver().value);

    auto debt_id_index = debts.get_index<name("bydebtid")>();

    auto iterator = debt_id_index.find(debt_id);

    check(iterator != debt_id_index.end(), "Debt does not exist");

    debt_id_index.erase(iterator);
  }
};