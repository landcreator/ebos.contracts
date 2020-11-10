#include <transrecords/transrecords.hpp>

namespace eosio{
  void transrecords::upsert(checksum256 trans_id, name from, name to, asset quantity, std::string memo, asset fee){
    require_auth(get_self());

    check( from != to, "cannot transfer to self" );
    check( is_account( from ), "from account does not exist");
    check( is_account( to ), "to account does not exist");

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( fee.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( fee.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == fee.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    transrecord_index transrecords(get_self(), get_first_receiver().value);

    auto trans_id_index = transrecords.get_index<name("bytransid")>();

    auto iterator = trans_id_index.find(trans_id);

    if( iterator == trans_id_index.end()){
      transrecords.emplace(get_self(), [&]( auto& row){
        row.pkey = transrecords.available_primary_key();
        row.trans_id = trans_id;
        row.from = from;
        row.to = to;
        row.quantity = quantity;
        row.memo = memo;
        row.fee = fee;
        row.timestamp = current_block_time();
      });
    }
    else{
      transrecords.modify(*iterator, get_self(), [&](auto& row){
        row.trans_id = trans_id;
        row.from = from;
        row.to = to;
        row.quantity = quantity;
        row.memo = memo;
        row.fee = fee;
        row.timestamp = current_block_time();
      });
    }
  }


  void transrecords::erase(checksum256 trans_id){
    require_auth(get_self());

    transrecord_index transrecords(get_self(), get_first_receiver().value);

    auto trans_id_index = transrecords.get_index<name("bytransid")>();

    auto iterator = trans_id_index.find(trans_id);

    check(iterator != trans_id_index.end(), "Transrecord does not exist");

    trans_id_index.erase(iterator);
  }
};