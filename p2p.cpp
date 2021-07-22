#include "p2p.hpp"

using namespace eosio;


void p2p::add_balance(eosio::name payer, eosio::asset quantity, eosio::name contract){
    require_auth(payer);

    balances_index balances(_me, payer.value);
    
    auto balances_by_contract_and_symbol = balances.template get_index<"byconsym"_n>();
    auto contract_and_symbol_index = combine_ids(contract.value, quantity.symbol.code().raw());

    auto balance = balances_by_contract_and_symbol.find(contract_and_symbol_index);

    if (balance  == balances_by_contract_and_symbol.end()){
      balances.emplace(_me, [&](auto &b) {
        b.id = balances.available_primary_key();
        b.contract = contract;
        b.quantity = quantity;
      }); 
    } else {
      balances_by_contract_and_symbol.modify(balance, _me, [&](auto &b) {
        b.quantity += quantity;
      });
    };
  
}

void p2p::sub_balance(eosio::name username, eosio::asset quantity, eosio::name contract){
    balances_index balances(_me, username.value);
    
    auto balances_by_contract_and_symbol = balances.template get_index<"byconsym"_n>();
    auto contract_and_symbol_index = combine_ids(contract.value, quantity.symbol.code().raw());

    auto balance = balances_by_contract_and_symbol.find(contract_and_symbol_index);
    
    eosio::check(balance != balances_by_contract_and_symbol.end(), "Balance is not found");
    
    eosio::check(balance -> quantity >= quantity, "Not enought user balance for create order");

    if (balance -> quantity == quantity) {

      balances_by_contract_and_symbol.erase(balance);

    } else {

      balances_by_contract_and_symbol.modify(balance, _me, [&](auto &b) {
        b.quantity -= quantity;
      });  

    }
    
}

uint64_t p2p::get_order_id(){
  counts_index counts(_me, _me.value);
  auto count = counts.find("totalorders"_n.value);
  uint64_t id = 1;

  if (count == counts.end()){
    counts.emplace(_me, [&](auto &c){
      c.key = "totalorders"_n;
      c.value = id;
    });
  } else {

    id = count -> value + 1;

    counts.modify(count, _me, [&](auto &c){
      c.value = id;
    });

  }

  return id;
}

void p2p::createorder(name username, uint64_t parent_id, name type, eosio::name root_contract, eosio::asset root_quantity, eosio::name quote_type, double quote_rate, eosio::name quote_contract, eosio::asset quote_quantity, eosio::name out_type, double out_rate, eosio::name out_contract, eosio::asset out_quantity, std::string details)
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    
    check(type == "buy"_n || type == "sell"_n, "Only buy or sell types");

    //CHECK for currency which can be used as a quote and out
    check(quote_contract == ""_n, "quote contract is not need to set");
    check(quote_quantity.symbol == eosio::symbol(eosio::symbol_code("USD"),4), "Wrong symbol as a QUOTE");
    check(asset(uint64_t(root_quantity.amount * quote_rate), quote_quantity.symbol) == quote_quantity, "root_quantity * quote_rate is not equal setted quote_quantity");
    check(out_contract == ""_n, "out contract is not need to set right now");
    check(out_type == "crypto"_n, "only crypto types is accepted as a out currency");
    check(quote_type == "external"_n, "only external quote currency is accepted for now");

    eosio::name parent_creator;

    if (parent_id != 0) {

      check(out_quantity.amount > 0, "out quantity should be more then zero");
      check(out_rate != 0, "out rate should be setted");
      
      auto parent_order = orders.find(parent_id);
      
      parent_creator = parent_order -> creator;
      check(parent_order != orders.end(), "Parent order is not found");
      check(parent_order -> parent_id == 0, "Parent order should not have a parent");
      check(parent_order -> root_remain >= root_quantity, "Not enought quantity in the parent order");
      check(parent_order -> root_contract == root_contract, "Wrong root contract in the parent order");
      check(parent_order -> quote_rate == quote_rate, "quote_rate is not equal to parent");
      check(parent_order -> quote_quantity.symbol == quote_quantity.symbol, "Quote symbol is not equal");
      check(parent_order -> quote_contract == quote_contract, "Quote contract is not equal");
      check(parent_order -> out_quantity.symbol == out_quantity.symbol, "Out symbols is not equal");
      check(parent_order -> out_contract == out_contract, "Out contracts is not equal");

      if (type == "sell"_n) {
        
        check(parent_order -> type == "buy"_n, "Wrong parent order type");
        
        sub_balance(username, root_quantity, root_contract);      
      
      } else if (type =="buy"_n) {
      
        check(parent_order -> type == "sell"_n, "Wrong parent order type");
        details = parent_order -> details;

      };
      
    } else {

      check(out_quantity.amount == 0, "out quantity should be a zero on the parent object");
      check(out_rate == 0, "out rate should be a zero");

      if (type == "sell"_n) {
        sub_balance(username, root_quantity, root_contract);
      } else if (type == "buy"_n) {
        // details = "";
      }

    };


    uint64_t id = p2p::get_order_id();

    if (id == 0) {id = 1;};

    orders.emplace(username, [&](auto &o) {
      o.id = id;
      o.parent_id = parent_id;
      o.curator = _curator;
      o.creator = username;
      o.parent_creator = parent_creator;
      o.type = type;
      
      o.root_symbol = root_quantity.symbol.code().to_string();
      o.root_contract = root_contract;
      o.root_precision = root_quantity.symbol.precision();
      o.root_quantity = root_quantity;
      o.root_remain = root_quantity;
      o.root_locked = asset(0, root_quantity.symbol);
      o.root_completed = asset(0, root_quantity.symbol);

      o.quote_type = quote_type;
      o.quote_rate = quote_rate;
      o.quote_symbol = quote_quantity.symbol.code().to_string(); 
      o.quote_precision = quote_quantity.symbol.precision();

      o.quote_quantity = quote_quantity;
      o.quote_locked = asset(0, quote_quantity.symbol);
      
      o.quote_remain = quote_quantity;
      o.quote_completed = asset(0, quote_quantity.symbol);


      o.out_symbol = out_quantity.symbol.code().to_string();
      o.out_contract = ""_n;
      o.out_type = out_type;
      o.out_rate = out_rate;
      
      o.out_precision = out_quantity.symbol.precision();
      o.out_quantity = out_quantity;
      o.out_locked = asset(0, out_quantity.symbol);
      o.out_remain = out_quantity;
      o.out_completed = asset(0, out_quantity.symbol);

      
      o.status = "waiting"_n;
      o.details = details;
      o.created_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
    });

}



void p2p::accept(name username, uint64_t id, std::string details) // подтверждение факта присутствия
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "waiting"_n, "Only order on waiting mode can be accepted");

    auto parent_order = orders.find(order -> parent_id);
    
    check(parent_order != orders.end(), "Parent order is not found");
    check(parent_order -> creator == username, "Only creator of the parent order can accept this order");

    
    eosio::asset root_quantity = order-> root_quantity;
    eosio::asset quote_quantity = order -> quote_quantity;
    eosio::asset out_quantity = order -> out_quantity;
    

    //перехлест ордеров
    if (parent_order -> root_remain < root_quantity){
      print("перехлест");
      root_quantity = parent_order -> root_remain;
      quote_quantity = parent_order -> quote_remain;

      double double_out_quantity = (double(order-> root_quantity.amount) - double(parent_order -> root_remain.amount)) / double(order-> root_quantity.amount) * double(order->out_quantity.amount);

      out_quantity = eosio::asset(uint64_t(double_out_quantity), out_quantity.symbol);
      
      if (order -> type == "sell"_n) {
        eosio::asset to_back = order-> root_quantity - parent_order -> root_remain;
        //transfer diff back to creator 
        action(
            permission_level{ _me, "active"_n },
            order->root_contract, "transfer"_n,
            std::make_tuple( _me, order->creator, to_back, std::string("p2p-back")) 
        ).send();
    
      }

    }

    //order    
    orders.modify(order, _me, [&](auto &o) {
      o.expired_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch() + _ORDER_EXPIRATION);
      o.status = "process"_n;
      
      check(order -> root_remain >= order -> root_quantity, "System Error 3");
      check(root_quantity.amount > 0, "Order is already completed.");
      o.root_quantity = root_quantity;
      
      o.root_remain -= root_quantity; 
      o.root_locked += root_quantity;
      
      o.quote_remain -= quote_quantity;
      o.quote_locked += quote_quantity;
      o.out_quantity = out_quantity;
      o.out_remain = asset(0, out_quantity.symbol);
      o.out_locked += out_quantity;

      if (order -> type == "buy"_n){
        o.details = details;
      };

    });

    //parent
    orders.modify(parent_order, username, [&](auto &p){
    
      check(parent_order -> root_remain >= root_quantity, "Not enought remain quantity for accept the order");
      
      p.root_remain -= root_quantity;
      p.root_locked += root_quantity;
      
      p.quote_remain -= quote_quantity;
      p.quote_locked += quote_quantity;
      // p.out_remain -= order -> out_quantity;
      // p.out_locked += order -> out_quantity;


    });
    
}


void p2p::confirm(name username, uint64_t id) //подтверждение факта платежа
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "process"_n, "Only order on process mode can be confirmed");

    auto parent_order = orders.find(order -> parent_id);
    
    check(parent_order != orders.end(), "Parent order is not found");
    
    if (parent_order -> type == "sell"_n) {
      check(order -> creator == username, "Only child order creator can confirm the payment");  
    } else if (parent_order -> type == "buy"_n) {
      check(parent_order -> creator == username, "Only parent order creator can confirm the payment");  
    }

    orders.modify(order, _me, [&](auto &o) {
      o.status = "payed"_n;
    });

}

void p2p::approve(name username, uint64_t id) //подтверждение успешного завершения сделки
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    
    auto order = orders.find(id);
    
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "payed"_n, "Only payed order can be approved");

    auto parent_order = orders.find(order -> parent_id);
    check(parent_order != orders.end(), "Parent order is not found");

    if (order -> type == "buy"_n) {
      eosio::check(parent_order -> creator == username, "Waiting approve from creator of parent order");

      action(
          permission_level{ _me, "active"_n },
          order->root_contract, "transfer"_n,
          std::make_tuple( _me, order->creator, order->root_quantity, std::string("p2p-buy")) 
      ).send();
    
    } else if (order -> type == "sell"_n) {
      eosio::check(order -> creator == username, "Waiting approve from creator of child order");
      
      action(
          permission_level{ _me, "active"_n },
          order->root_contract, "transfer"_n,
          std::make_tuple( _me, parent_order->creator, order->root_quantity, std::string("p2p-sell")) 
      ).send();
      
    } else {

      check(false, "System error 1");
    
    }

    orders.modify(order, _me, [&](auto &o) {
      check(order -> root_locked >= order -> root_quantity, "System error 5");

      o.status = "finish"_n;
      o.expired_at = eosio::time_point_sec(0);
      o.root_completed += order -> root_quantity;
      o.root_locked -= order -> root_quantity;

      o.quote_completed += order -> quote_quantity;
      o.quote_locked -= order -> quote_quantity;
      o.out_completed += order -> out_quantity;
      o.out_locked -= order -> out_quantity;

    });

    orders.modify(parent_order, _me, [&](auto &o) {

      eosio::check(parent_order -> root_locked >= order -> root_quantity, "System error 2");
      
      o.root_locked -= order -> root_quantity;
      o.root_completed += order -> root_quantity; 

      o.quote_completed += order -> quote_quantity;
      o.quote_locked -= order -> quote_quantity;
      o.out_completed += order -> out_quantity;
      // o.out_locked -= order -> out_quantity;


      if (o.root_completed == parent_order -> root_quantity) {
        o.status = "finish"_n;
      };

    });

    //DELETE orders if finish
}



void p2p::cancel(name username, uint64_t id)
{
    require_auth( username );
    
    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
        
    if (order -> parent_id == 0) {

      eosio::check(order -> creator == username, "Only creator can cancel order");
    
      if ((order -> status == "waiting"_n)){ 

        eosio::check(order -> root_locked.amount == 0, "Can not cancel order with locked amounts");

        if (order -> type == "sell"_n) {
          
          if (order->root_remain.amount > 0){

            action (
                permission_level{ _me, "active"_n },
                order->root_contract, "transfer"_n,
                std::make_tuple( _me, order->creator, order->root_remain, std::string("p2p-cancel")) 
            ).send();

          };

        }

        orders.erase(order);

      } else {check(false, "Order is canceled or finished already");}

    } else {
      
      check(order -> status == "process"_n || order -> status == "waiting"_n, "Order can not be canceled now.");
      
      auto parent_order = orders.find(order -> parent_id);

      if (order -> status == "process"_n) { 

        check(order -> type == "buy"_n, "Only child buy order can be canceled on process status");

        eosio::check(order -> creator == username || parent_order -> creator == username, "Only creator can cancel the order");
        
        if (parent_order -> creator == username) {
          eosio::check(order->expired_at < eosio::time_point_sec(eosio::current_time_point().sec_since_epoch()), "Order is not expired yet for be a canceled from the parent owner");
        };

        orders.modify(parent_order, username, [&](auto &o){
          o.root_remain += order -> root_quantity;
          o.root_locked -= order -> root_quantity;  

          o.quote_remain += order -> quote_quantity;
          o.quote_locked -= order -> quote_quantity;
          // o.out_remain += order -> out_quantity;
          // o.out_locked -= order -> out_quantity;

        });

      } else if (order -> status == "waiting"_n) {
        //Если родительский ордер заполнен, то позволяем отменить его любой стороне сделки.
        if (parent_order -> root_remain.amount == 0) {
          eosio::check(has_auth(order->creator) || has_auth(order->parent_creator), "missing required authority");
        } else {
          eosio::check(order -> creator == username, "Only creator can cancel order2");
        }


        if (order -> type == "sell"_n) {
          action (
              permission_level{ _me, "active"_n },
              order->root_contract, "transfer"_n,
              std::make_tuple( _me, order->creator, order->root_remain, std::string("p2p-cancel")) 
          ).send(); 
        };
      };

      orders.erase(order);

      //TODO: IF PARENT ORDER IS EXPIRED AND CANCELED BY CHILD - DELETE IT


    } 
}


void p2p::dispute(name username, uint64_t id) //открытие спора
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "payed"_n, "Only order on payed status can be disputted");

    auto parent_order = orders.find(order -> parent_id);
    
    check(parent_order != orders.end(), "Parent order is not found");
    
    if (parent_order -> type == "buy"_n) {
      check(order -> creator == username, "Only child order creator can open dispute");  
    } else if (parent_order -> type == "sell"_n) {
      check(parent_order -> creator == username, "Only parent order creator can open dispute");  
    }

    orders.modify(order, _me, [&](auto &o) {
      o.status = "dispute"_n;
    });

}

void p2p::del(name username, uint64_t id) //удаление завершенной сделки из оперативной памяти
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "finish"_n, "Only order on finish status can be deleted");

    orders.erase(order);

}

void p2p::uprate(eosio::name out_contract, eosio::asset out_asset){
  
  require_auth( "eosio"_n );
  
  usdrates_index usd_rates(_me, _me.value);

  auto rates_by_contract_and_symbol = usd_rates.template get_index<"byconsym"_n>();
  auto contract_and_symbol_index = combine_ids(out_contract.value, out_asset.symbol.code().raw());
  
  eosio::check(out_contract == "eosio.token"_n && out_asset.symbol == _SYM, "Only system asset can be used for uprate");

  auto usd_rate = rates_by_contract_and_symbol.find(contract_and_symbol_index);

  if (usd_rate == rates_by_contract_and_symbol.end()) {

      usd_rates.emplace("eosio"_n, [&](auto &r){

        r.id = usd_rates.available_primary_key();
        r.out_contract = out_contract;
        r.out_asset = out_asset;
        r.rate = 1;
        r.updated_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());

      });
    
    } else {

      double rate = usd_rate -> rate  + (double(eosio::current_time_point().sec_since_epoch() - usd_rate -> updated_at.sec_since_epoch() ) / (double)86400 * (double)_PERCENTS_PER_MONTH / (double)30 / (double)100);

      rates_by_contract_and_symbol.modify(usd_rate, "eosio"_n, [&](auto &r){
      
        r.rate = rate;
        r.updated_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
      
      }); 

      
      usdrates2_index usdrates2(_me, _me.value);
      auto usdrate2 = usdrates2.find(usd_rate -> id);

      if (usdrate2 == usdrates2.end()){
        usdrates2.emplace(_me, [&](auto &ur2){
          ur2.id = usd_rate -> id;
          ur2.created_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
        });
      }


    }


}

void p2p::setrate(eosio::name out_contract, eosio::asset out_asset, double rate)
{
    require_auth(_rater); 

    eosio::check(out_contract == ""_n, "Out contract is not supported now");  

    usdrates_index usd_rates(_me, _me.value);

    auto rates_by_contract_and_symbol = usd_rates.template get_index<"byconsym"_n>();
    auto contract_and_symbol_index = combine_ids(out_contract.value, out_asset.symbol.code().raw());

    auto usd_rate = rates_by_contract_and_symbol.find(contract_and_symbol_index);

    if (usd_rate == rates_by_contract_and_symbol.end()) {
      usd_rates.emplace(_rater, [&](auto &r){
        r.id = usd_rates.available_primary_key();
        r.out_contract = out_contract;
        r.out_asset = out_asset;
        r.rate = rate;
        r.updated_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
      });
    } else {

      rates_by_contract_and_symbol.modify(usd_rate, _rater, [&](auto &r){
        r.rate = rate;
        r.updated_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
      });    

    }
    

}

extern "C" {
   
   /// The apply method implements the dispatch of events to this contract
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
        
        if (code == p2p::_me.value) {

          if (action == "createorder"_n.value){
            execute_action(name(receiver), name(code), &p2p::createorder);
          } else if (action == "accept"_n.value){
            execute_action(name(receiver), name(code), &p2p::accept);
          } else if (action == "confirm"_n.value){
            execute_action(name(receiver), name(code), &p2p::confirm);
          } else if (action == "approve"_n.value){
            execute_action(name(receiver), name(code), &p2p::approve);
          } else if (action == "cancel"_n.value){
            execute_action(name(receiver), name(code), &p2p::cancel);
          } else if (action == "dispute"_n.value){
            execute_action(name(receiver), name(code), &p2p::dispute);
          } else if (action == "del"_n.value){
            execute_action(name(receiver), name(code), &p2p::del);
          } else if (action == "setrate"_n.value){
            execute_action(name(receiver), name(code), &p2p::setrate);
          } else if (action == "uprate"_n.value){
            execute_action(name(receiver), name(code), &p2p::uprate);
          } 




        } else {
          if (action == "transfer"_n.value){
            
            struct transfer{
                eosio::name from;
                eosio::name to;
                eosio::asset quantity;
                std::string memo;
            };

            auto op = eosio::unpack_action_data<transfer>();
            if (op.to == p2p::_me){

              p2p::add_balance(op.from, op.quantity, eosio::name(code));
            }
          }
        }
  };
};