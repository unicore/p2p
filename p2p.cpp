#include "p2p.hpp"

using namespace eosio;
/**
\defgroup public_consts CONSTS
\brief Константы контракта
*/

/**
\defgroup public_actions ACTIONS
\brief Методы действий контракта
*/

/**
\defgroup public_tables TABLES
\brief Структуры таблиц контракта
*/


void p2p::check_guest_and_gift_account(eosio::name username, eosio::name contract, eosio::asset amount, uint64_t gift_account_from_amount){
    

    if (amount.amount >= gift_account_from_amount) {

      guests_index guests(_REGISTRATOR_ACCOUNT, _REGISTRATOR_ACCOUNT.value);

      auto guest = guests.find(username.value);

      if (guest != guests.end()) {

        bbonuses_index bonuses(_me, _me.value);
        auto bonuse_bal = bonuses.find(_CORE_SALE_ACCOUNT.value);
        
        if (bonuse_bal -> available >= guest -> to_pay && bonuse_bal -> contract == contract) {

          std::string p2p_memo =  std::string(_me.to_string());

          action(
              permission_level{ _me, "active"_n },
              bonuse_bal->contract, "transfer"_n,
              std::make_tuple( _me, _REGISTRATOR_ACCOUNT, guest->to_pay, p2p_memo) 
          ).send();
         
          action(
              permission_level{ _me, "active"_n },
              _REGISTRATOR_ACCOUNT, "payforguest"_n,
              std::make_tuple( _me, username, guest -> to_pay) 
          ).send();
          
          bonuses.modify(bonuse_bal, _me, [&](auto &b) {
            b.available -= guest -> to_pay;
            b.distributed += guest -> to_pay;
          });

        }

      }
      
    }

};

void p2p::make_vesting_action(eosio::name owner, eosio::name contract, eosio::asset amount){
    eosio::check(amount.is_valid(), "Amount not valid");
    // eosio::check(amount.symbol == _SYM, "Not valid symbol for this vesting contract");
    eosio::check(is_account(owner), "Owner account does not exist");
    
    vesting_index vests (_me, owner.value);
    
    params_index params(_me, _me.value);
    auto pm = params.find(0);
    eosio::check(pm != params.end(), "Contract is not enabled");


    vests.emplace(_me, [&](auto &v) {
      v.id = vests.available_primary_key();
      v.owner = owner;
      v.contract = contract;
      v.amount = amount;
      v.available = asset(0, amount.symbol);
      v.withdrawed = asset(0, amount.symbol);
      v.startat = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
      v.duration = pm->vesting_seconds;
    });

  }

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


//Бонусный баланс пополняется только прямым переводом от хоста
void p2p::addbbal(eosio::name host, eosio::name contract, eosio::asset quantity){
    require_auth(host);

    bbonuses_index bonuses(_me, _me.value);
    
    auto bonuse_bal = bonuses.find(host.value);

    if (bonuse_bal  == bonuses.end()) {

      bonuses.emplace(_me, [&](auto &b) {
        b.host = host;
        b.contract = contract;
        b.total = quantity;
        b.available = quantity;
        b.distributed = asset(0, quantity.symbol);
        b.distribution_rate = 0;
      }); 

    } else {
      eosio::check(bonuse_bal -> contract == contract, "Wrong contract for add to bonuse balance");

      bonuses.modify(bonuse_bal, _me, [&](auto &b){
        b.total += quantity;
        b.available += quantity;
      });

    };
}


//Бонусный баланс уменьшается путём перевода на аккаунт ядра на распределение в момент покупки здесь
void p2p::subbbal(eosio::name host, eosio::name contract, eosio::asset quantity){
    require_auth(host);

    bbonuses_index bonuses(_me, _me.value);
    
    auto bonuse_bal = bonuses.find(host.value);

    if (bonuse_bal  == bonuses.end()) {

      eosio::check(false, "Cannot spread bonuse balance without balance");

    } else {
      // eosio::check(bonuse_bal -> contract == contract, "Wrong contract for bonuse balance");

      bonuses.erase(bonuse_bal);
      // bonuses.modify(bonuse_bal, _me, [&](auto &b) {
      //   b.available -= quantity;
      //   b.distributed += quantity;
      // });

    };
}


/**
 * @brief      Метод установки бонусного курса
 * @details    Вызывается владельцем бонусного баланса в контракте для подключения распределения на партнёрскую сеть покупателя.
 * @ingroup public_actions
 * @auth       host
 * @param[in]  host               The host
 * @param[in]  distribution_rate  The distribution rate
 */
void p2p::setbrate(eosio::name host, double distribution_rate) {

    require_auth(host);

    bbonuses_index bonuses(_me, _me.value);
    
    auto bonuse_bal = bonuses.find(host.value);

    eosio::check(bonuse_bal != bonuses.end(), "Bonus balance is not exist");
    
    bonuses.modify(bonuse_bal, _me, [&](auto &b) {
      b.distribution_rate = distribution_rate;
    });

}

//Установка курсу конвертации в бонусы
void p2p::check_bonuse_system(eosio::name creator, eosio::name reciever, eosio::asset quantity) {

    bbonuses_index bonuses(_me, _me.value);
    
    auto bonuse_bal = bonuses.find(creator.value);

    if (bonuse_bal != bonuses.end()) {
     
      eosio::asset to_distribution = asset(quantity.amount * bonuse_bal -> distribution_rate, quantity.symbol);
      
      account_index accounts(_core, creator.value);
      auto acc = accounts.find(creator.value);
      uint64_t to_ref_percent = _HUNDR_PERCENT;
      uint64_t to_dac_percent = 0;
      eosio::asset to_ref_amount = to_distribution;
      eosio::asset to_dac_amount = asset(0, quantity.symbol); 


      if (acc != accounts.end()) {
         to_ref_percent = acc -> referral_percent;
         to_dac_percent = _HUNDR_PERCENT - to_ref_percent;
         to_ref_amount = to_distribution * to_ref_percent / _HUNDR_PERCENT;
         to_dac_amount = to_distribution * to_dac_percent / _HUNDR_PERCENT;
      }

      if (to_ref_amount <= bonuse_bal -> available && to_ref_amount.amount > 0) {
        
        std::string st1 = "111";
        std::string st2 = std::string(creator.to_string());
        std::string st3 = "-";
        std::string st4 = std::string(reciever.to_string());

        std::string st = std::string(st1 + st3 + st2 + st3 + st4);
        
        action(
            permission_level{ _me, "active"_n },
            bonuse_bal->contract, "transfer"_n,
            std::make_tuple( _me, _core, to_ref_amount, st) 
        ).send();

        
      }

      if (to_dac_amount <= bonuse_bal -> available && to_dac_amount.amount > 0) {
        
        std::string st1 = "222";
        std::string st2 = std::string(creator.to_string());
        std::string st3 = "-";
        std::string st4 = std::string(reciever.to_string());

        std::string st = std::string(st1 + st3 + st2);
        
        action(
            permission_level{ _me, "active"_n },
            bonuse_bal->contract, "transfer"_n,
            std::make_tuple( _me, _core, to_dac_amount, st) 
        ).send();

        
      }
            
      if (to_ref_amount.amount > 0 || to_dac_amount.amount > 0)
        bonuses.modify(bonuse_bal, _me, [&](auto &b) {
            b.available -= (to_ref_amount + to_dac_amount);
            b.distributed += to_ref_amount + to_dac_amount;
        });  


    }
    
    
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



/**
 * @brief      Метод создания ордера
 * 
 
 * @details    Используя метод, пользователь создаёт ордер для обмена. 
 * @auth    username
 * @ingroup public_actions
 * @param[in]  username        Имя пользователя, инициирующего обмен
 * @param[in]  parent_id       Опциональный идентификатор родительской сделки, с которой происходит обмен. Если не установлен - ордер ожидает появление дочерних ордеров в статусе waiting, предлагающих обмен. 
 * @param[in]  type            Тип ордера buy / sell
 * @param[in]  root_contract   Имя контракта токена обмена
 * @param[in]  root_quantity   Количество токенов на обмене
 * @param[in]  quote_type      Тип опорной валюты, сейчас используем только тип external
 * @param[in]  quote_rate      Опорный курс обмена
 * @param[in]  quote_contract  Опорный контракт обмена, сейчас используем только ""
 * @param[in]  quote_quantity  Количество опорной валюты на обмене, измеряемой в USD
 * @param[in]  out_type        Тип валюты выхода (сейчас любой)
 * @param[in]  out_rate        Курс валюты выхода
 * @param[in]  out_contract    Контракт валюты выхода (сейчас только "")
 * @param[in]  out_quantity    Количество валюты выхода
 * @param[in]  details         Реквизиты для получения платежа, используются если тип нового ордера = buy
 */
void p2p::createorder(name username, uint64_t parent_id, name type, eosio::name root_contract, eosio::asset root_quantity, eosio::name quote_type, double quote_rate, eosio::name quote_contract, eosio::asset quote_quantity, eosio::name out_type, double out_rate, eosio::name out_contract, eosio::asset out_quantity, std::string details)
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    
    params_index params(_me, _me.value);
    auto pm = params.find(0);
    eosio::check(pm != params.end(), "Contract is not enabled");


    check(type == "buy"_n || type == "sell"_n, "Only buy or sell types");

    //CHECK for currency which can be used as a quote and out
    check(quote_contract == ""_n, "quote contract is not need to set");
    check(quote_quantity.symbol == eosio::symbol(eosio::symbol_code("USD"),4), "Wrong symbol as a QUOTE");
    
    // check(asset(uint64_t(root_quantity.amount * quote_rate), quote_quantity.symbol) <= quote_quantity, "root_quantity * quote_rate is not equal setted quote_quantity");
    check(out_contract == ""_n, "out contract is not need to set right now");
    // check(out_type == "crypto"_n, "only crypto types is accepted as a out currency");
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
      // check(parent_order -> quote_rate == quote_rate, "quote_rate is not equal to parent");
      check(parent_order -> quote_quantity.symbol == quote_quantity.symbol, "Quote symbol is not equal");
      check(parent_order -> quote_contract == quote_contract, "Quote contract is not equal");
      check(parent_order -> out_quantity.symbol == out_quantity.symbol, "Out symbols is not equal");
      check(parent_order -> out_contract == out_contract, "Out contracts is not equal");

      //TODO check for exist for current user
      // auto orders_by_parent_id_and_creator = orders.template get_index<"byparandus"_n>();
      // auto orders_by_parent_id_and_creator_index = combine_ids(parent_id, username.value);
      // auto exist = orders_by_parent_id_and_creator.find(orders_by_parent_id_and_creator_index);
      // eosio::check(exist == orders_by_parent_id_and_creator.end(), "Order with current parent order is already exist. Cancel or finish it first.");

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
      // o.out_type = out_type;
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

    //WARN DO NOT DELETE THIS! USEFUL FOR PRODUCTION ON BACKEND
    print("ORDER_ID:", id);

}



/**
 * @brief      Метод подтверждения факта присутствия и начало сделки
 * @ingroup public_actions
 * @details Реквизиты для оплаты передаются в поле details, если дочерний ордер типа buy. 
 * Статус ордера изменяется на process. 
 * @auth username
 
 * @param[in]  username  имя пользователя, подтверждающего начало сделки
 * @param[in]  id        идентификатор ордера
 * @param[in]  details   реквизиты для получения оплаты по сделке
 */
void p2p::accept(name username, uint64_t id, std::string details) // подтверждение факта присутствия
{
    
    if (!has_auth(username) ) {
      require_auth( _CORE_SALE_ACCOUNT );
    } else {
      require_auth( username );
    } 


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
            std::make_tuple( _me, order->creator, to_back, std::string("p2pback")) 
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
    orders.modify(parent_order, _me, [&](auto &p){
    
      check(parent_order -> root_remain >= root_quantity, "Not enought remain quantity for accept the order");
      
      p.root_remain -= root_quantity;
      p.root_locked += root_quantity;
      
      p.quote_remain -= quote_quantity;
      p.quote_locked += quote_quantity;
      // p.out_remain -= order -> out_quantity;
      // p.out_locked += order -> out_quantity;


    });
    
}


/**
 * @brief      Метод подтверждения факта платежа
 * @ingroup public_actions
 
 * @details После совершения оплаты на реквизиты, участник сделки должен подтвердить этот факт вызовом этого метода.
 * Статус ордера изменяется на payed. 
 * @auth username
 * @param[in]  username       имя аккаунта участника сделки, утверждающего факт совершения оплаты на реквизиты
 * @param[in]  id       идентификатор ордера
 */
void p2p::confirm(name username, uint64_t id)
{
    if (!has_auth(username) ) {
      require_auth( _CORE_SALE_ACCOUNT );
    } else {
      require_auth( username );
    } 


    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "process"_n || order -> status == "payed"_n, "Only order on process mode can be confirmed");

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



/**
 * @brief      Метод утверждения завершенного ордера
 * @ingroup public_actions
 
 * @details После получения оплаты, пользователь должен вызовать этот метод и утвердить завершение сделки, разблокировав токены для второго участника сделки.
 * Статус ордера изменяется на finish и производится разблокирование токенов для получателя.
 * @auth username
 
 * @param[in]  username       имя аккаунта участника сделки, утверждающего успешное завершение сделки
 * @param[in]  id       идентификатор ордера
 */
void p2p::approve(name username, uint64_t id) 
{
    if (!has_auth(username) ) {
      require_auth( _CORE_SALE_ACCOUNT );
    } else {
      require_auth( username );
    } 

    orders_index orders(_me, _me.value);
  
    params_index params(_me, _me.value);
    auto pm = params.find(0);
    
    auto order = orders.find(id);
    
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "payed"_n, "Only payed order can be approved");

    auto parent_order = orders.find(order -> parent_id);
    check(parent_order != orders.end(), "Parent order is not found");

    if (order -> type == "buy"_n) {

      eosio::check(parent_order -> creator == username, "Waiting approve from creator of parent order");

      if ( pm->enable_vesting == false) {

        action(
            permission_level{ _me, "active"_n },
            order->root_contract, "transfer"_n,
            std::make_tuple( _me, order->creator, order->root_quantity, std::string("p2pbuy")) 
        ).send();
      
      } else {

        if (parent_order->creator == _CORE_SALE_ACCOUNT) {
       
          make_vesting_action(order->creator, order->root_contract, order->root_quantity);
          //TODO check for guest and gift account to user
          check_guest_and_gift_account(order->creator, order->root_contract, order->root_quantity, pm->gift_account_from_amount);

        } else {

          action(
            permission_level{ _me, "active"_n },
            order->root_contract, "transfer"_n,
            std::make_tuple( _me, order->creator, order->root_quantity, std::string("p2pbuy")) 
          ).send();

        }
        

      };
  
      //parent creator should pay gifts if has possibility
      //child order creator recieve referral gifts
      check_bonuse_system(order->parent_creator, order->creator, order->root_quantity);
    
    } else if (order -> type == "sell"_n) {
      eosio::check(order -> creator == username, "Waiting approve from creator of child order");
      
      if (pm->enable_vesting == false) {

        action(
          permission_level{ _me, "active"_n },
          order->root_contract, "transfer"_n,
          std::make_tuple( _me, parent_order->creator, order->root_quantity, std::string("p2psell")) 
        ).send();
      
      } else {

        if (parent_order->creator == _CORE_SALE_ACCOUNT) {
        
          make_vesting_action(parent_order->creator, order->root_contract, order->root_quantity);
          check_guest_and_gift_account(parent_order->creator, order->root_contract, order->root_quantity, pm->gift_account_from_amount);

        } else {

          action(
            permission_level{ _me, "active"_n },
            order->root_contract, "transfer"_n,
            std::make_tuple( _me, parent_order->creator, order->root_quantity, std::string("p2psell")) 
          ).send();
            
        }
        

      }
      
      check_bonuse_system(order->creator, parent_order->creator, order->root_quantity);
      

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



/**
 * @brief      Метод отмены ордера
 * @ingroup public_actions
 * @details Отменяет ордер и удаляет его из оперативной памяти, разблокируя средства согласну типу ордера. 
 * @auth username
 
 * @param[in]  username       имя аккаунта участника сделки, производящего отмену ордера. 
 * @param[in]  id       идентификатор ордера
 */
void p2p::cancel(name username, uint64_t id)
{
    // require_auth( username );
    eosio::check(has_auth(username) || has_auth(_me), "missing required authority");
       
    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
    
    if (order -> parent_id == 0) {

      eosio::check(order -> creator == username || has_auth(_me), "Only creator can cancel order");
    
      if ((order -> status == "waiting"_n)) { 
        eosio::check(order -> root_locked.amount == 0, "Can not cancel order with locked amounts");

        if (order -> type == "sell"_n) {

          if (order->root_remain.amount > 0) {

            action (
                permission_level{ _me, "active"_n },
                order->root_contract, "transfer"_n,
                std::make_tuple( _me, order->creator, order->root_remain, std::string("p2pcancel")) 
            ).send();

          };

        }

        orders.erase(order);

      } else if (order -> status == "finish"_n){
        //nothing, just erase next
        orders.erase(order);

      } else {check(false, "Order is canceled or finished already");}

    } else {
      
      // check(order -> status == "process"_n || order -> status == "waiting"_n || order -> status == "payed"_n, "Order can not be canceled now.");
      
      auto parent_order = orders.find(order -> parent_id);

      if (order -> status == "process"_n || order -> status == "payed"_n) { 

        check(order -> type == "buy"_n, "Only child buy order can be canceled on process or payed status");

        eosio::check(order -> creator == username || parent_order -> creator == username, "Only creator can cancel the order");
        
        if (parent_order -> creator == username) {
          eosio::check(order->expired_at < eosio::time_point_sec(eosio::current_time_point().sec_since_epoch()), "Order is not expired yet for be a canceled from the parent owner");
        };

        orders.modify(parent_order, _self, [&](auto &o){
          o.root_remain += order -> root_quantity;
          o.root_locked -= order -> root_quantity;  

          o.quote_remain += order -> quote_quantity;
          o.quote_locked -= order -> quote_quantity;
          // o.out_remain += order -> out_quantity;
          // o.out_locked -= order -> out_quantity;

        });

      } else if (order -> status == "waiting"_n) {
        // if (parent_order -> root_remain.amount == 0) {
          eosio::check(has_auth(order->creator) || has_auth(order->parent_creator) || has_auth(_me), "missing required authority");
        // } else {
          // eosio::check(order -> creator == username, "Only creator can cancel order2");
        // }


        if (order -> type == "sell"_n) {
          action (
              permission_level{ _me, "active"_n },
              order->root_contract, "transfer"_n,
              std::make_tuple( _me, order->creator, order->root_remain, std::string("p2pcancel")) 
          ).send(); 
        };
      } else if (order -> status == "finish"_n){

        eosio::check(order -> creator == username, "Only creator can cancel this order");
        
      }

      orders.erase(order);

      //TODO: IF PARENT ORDER IS EXPIRED AND CANCELED BY CHILD - DELETE IT


    } 
}


/**
 * @brief      Метод создания спора
 * @ingroup public_actions
 
 * @details Перевод сделку в статус спора, блокируя вывод средств до его разрешения. Только сделка в статусе payed может быть переведена в статус спора. 
 * @auth username
 * @param[in]  username       имя аккаунта участника сделки, инициирующего спор
 * @param[in]  id       идентификатор ордера
 */
void p2p::dispute(name username, uint64_t id) 
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


/**
 * @brief      Метод удаления завершенной сделки из оперативной памяти
 * @ingroup public_actions
 
 * @details Очищает завершенную сделку из оперативной памяти операционной системы.
 * @auth username
 * @param[in]  username       имя аккаунта, производящего очищение
 * @param[in]  id       идентификатор ордера
 */
void p2p::del(name username, uint64_t id) 
{
    require_auth( username );

    orders_index orders(_me, _me.value);
    auto order = orders.find(id);
    eosio::check(order != orders.end(), "Order is not found");
    eosio::check(order -> status == "finish"_n, "Only order on finish status can be deleted");

    orders.erase(order);

}


/**
 * @brief      Метод удаление курса
 * @ingroup public_actions
 
 * @details Используется администратором для удаления курса из таблицы курсов
 * @auth eosio
 * @param[in]  id       идентификатор курса
 */
void p2p::delrate(uint64_t id){
  
  require_auth( "eosio"_n );
  usdrates_index usd_rates(_me, _me.value);

  auto rate = usd_rates.find(id);
  usd_rates.erase(rate);

}


/**
 * @brief      Метод удаление вестинг-баланса
 * @ingroup public_actions
 
 * @details Используется администратором для удаления ошибочного начисления вестинг-баланса
 * @auth p2p
 * @param[in]  owner    имя аккаунта владельца вестинг-баланса
 * @param[in]  id       идентификатор вестинг-баланса
 */
void p2p::delvesting(eosio::name owner, uint64_t id){
  
  require_auth( "p2p"_n );
  vesting_index vests(_me, owner.value);
  auto v = vests.find(id);
  eosio::check(v != vests.end(), "Vesting object does not exist");
  
  vests.erase(v); 
}


/**
 * @brief      Метод увеличения курса обмена системного токена
 * @ingroup public_actions
 
 * @details Периодически вызывается системным контрактом и увеличивает курс обмена системного токена согласно темпу роста в pm->percents_per_month.
 * @auth eosio
 * @param[in]  out_contract    имя контракта выхода (обычно "eosio.token")
 * @param[in]  out_asset       токен выхода с точностью и символом (обычно равен _SYM)
 */

void p2p::uprate(eosio::name out_contract, eosio::asset out_asset){
  
  require_auth( "eosio"_n );
  params_index params(_me, _me.value);
  auto pm = params.find(0);

  if (pm != params.end() && pm -> enable_growth == true) {

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
          r.rate = pm -> start_rate;
          r.updated_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());

        });
      
      } else {

        //LINEAR PER MONTH
        double rate = usd_rate -> rate  + pm->start_rate * (double(eosio::current_time_point().sec_since_epoch() - usd_rate -> updated_at.sec_since_epoch() ) / (double)86400 * (double)pm->percents_per_month / (double)30 / (double)100);

        //DOWNGRADE PER MONTH

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

}


/**
   * @brief      Метод установки курса обмена к USD
   * @ingroup public_actions
   * @details Устанавливает в таблицу usdrates новый актуальный курс от поставщика.
   * @auth _rater | _me
   * @param[in]  out_contract    имя контракта выхода (обычно "")
   * @param[in]  out_asset       токен выхода с точностью и символом
   * @param[in]  rate            курс обмена к USD
   */
void p2p::setrate(eosio::name out_contract, eosio::asset out_asset, double rate)
{
    
    // eosio::check(out_contract == ""_n, "Out contract is not supported now");  

    usdrates_index usd_rates(_me, _me.value);

    auto rates_by_contract_and_symbol = usd_rates.template get_index<"byconsym"_n>();
    auto contract_and_symbol_index = combine_ids(out_contract.value, out_asset.symbol.code().raw());

    auto usd_rate = rates_by_contract_and_symbol.find(contract_and_symbol_index);

    if (usd_rate == rates_by_contract_and_symbol.end()) {
      usd_rates.emplace(_me, [&](auto &r){
        r.id = usd_rates.available_primary_key();
        r.out_contract = out_contract;
        r.out_asset = out_asset;
        r.rate = rate;
        r.updated_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
      });
    } else {

      rates_by_contract_and_symbol.modify(usd_rate, _me, [&](auto &r){
        r.rate = rate;
        r.updated_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
      });    

    }
}





  /**
   * @brief      Метод обновления вестинг-баланса.  
   * @ingroup public_actions
   
   * @details Обновляет вестинг-баланс до доступного остатка. 
   * @auth owner
   * @param[in]  owner    имя аккаунта владельца вестинг-баланса
   * @param[in]  id       идентификатор вестинг-баланса
   */
  [[eosio::action]] void p2p::refreshsh (eosio::name owner, uint64_t id){
    require_auth(owner);
    vesting_index vests(_me, owner.value);
    params_index params(_me, _me.value);
    
    auto pm = params.find(0);
    eosio::check(pm != params.end(), "Contract is not activated");

    auto v = vests.find(id);
    eosio::check(v != vests.end(), "Vesting object does not exist");
    
    uint64_t now_secs = eosio::current_time_point().sec_since_epoch() ;
    uint64_t full_freeze_until_secs = pm->vesting_pause_until.sec_since_epoch() ;
    print("now_secs > v->startat.sec_since_epoch() ", now_secs > v->startat.sec_since_epoch());
    print("now_secs > full_freeze_until_secs ", now_secs > full_freeze_until_secs);
    
    if (now_secs > v->startat.sec_since_epoch() && now_secs > full_freeze_until_secs){
      
      auto elapsed_seconds = (eosio::time_point_sec(eosio::current_time_point().sec_since_epoch()) - v->startat).to_seconds();
      eosio::asset available;
    
      if( elapsed_seconds < v->duration){
        available = v->amount * elapsed_seconds / v->duration;
      } else {
        available = v->amount;
      }

      available = available - v->withdrawed;
      vests.modify(v, owner, [&](auto &m){
        m.available = available;
      });

    }
  }


  [[eosio::action]] void p2p::setparams(bool enable_growth, eosio::name growth_type, double start_rate, uint64_t percents_per_month, bool enable_vesting, uint64_t vesting_seconds, eosio::time_point_sec vesting_pause_until, uint64_t gift_account_from_amount, eosio::name ref_pay_type){
    require_auth(_me);

    params_index params(_me, _me.value);
    auto pm = params.find(0);
    
    if (pm == params.end()) {

      
      params.emplace(_me, [&](auto &p) {
        p.id = 0;
        p.enable_growth = enable_growth;
        p.growth_type = growth_type;
        p.start_rate = start_rate;
        p.percents_per_month = percents_per_month;
        p.enable_vesting = enable_vesting;
        p.vesting_seconds = vesting_seconds;
        p.vesting_pause_until = vesting_pause_until;
        p.gift_account_from_amount = gift_account_from_amount;
        p.ref_pay_type = ref_pay_type;
      });

    } else {

      params.modify(pm, _me, [&](auto &p){
        p.enable_growth = enable_growth;
        p.growth_type = growth_type;
        p.start_rate = start_rate;
        p.percents_per_month = percents_per_month;
        p.enable_vesting = enable_vesting;
        p.vesting_seconds = vesting_seconds;
        p.vesting_pause_until = vesting_pause_until;
        p.gift_account_from_amount = gift_account_from_amount;
        p.ref_pay_type = ref_pay_type;
      });

    }


    
  }


  /**
   * @brief      Вывод вестинг-баланса
   * @ingroup public_actions
   
   * @details Обеспечивает вывод доступных средств из вестинг-баланса. 
   * @auth owner
   * @param[in]  owner    имя аккаунта владельца вестинг-баланса
   * @param[in]  id       идентификатор вестинг-баланса
   */
  [[eosio::action]] void p2p::withdrawsh(eosio::name owner, uint64_t id){
    require_auth(owner);
    vesting_index vests(_me, owner.value);
    auto v = vests.find(id);
    eosio::check(v != vests.end(), "Vesting object does not exist");
    eosio::check((v->available).amount > 0, "Only positive amount can be withdrawed");
    
    action(
        permission_level{ _me, "active"_n },
        v->contract, "transfer"_n,
        std::make_tuple( _me, owner, v->available, std::string("Vesting Withdrawed")) 
    ).send();

    vests.modify(v, owner, [&](auto &m){
        m.withdrawed = v->withdrawed + v->available;
        m.available = eosio::asset(0, (v->available).symbol);
      });

    if (v->withdrawed == v->amount){
      vests.erase(v);
    };
    
  };


  [[eosio::action]] void p2p::fixrate(uint64_t id) {
    require_auth(_me);
    
    usdrates2_index usdrates2(_me, _me.value);
    
    auto usdrate2 = usdrates2.find(id);

    if (usdrate2 != usdrates2.end()) {
      usdrates2.modify(usdrate2, _me, [&](auto &ur2) {
        ur2.created_at = eosio::time_point_sec(eosio::current_time_point().sec_since_epoch());
      });
    }
    
    
  };


  

extern "C" {
   
   /**
    * @brief      Диспатчер контракта для распределения вызовов действий.
    *
    * @param[in]  receiver  The receiver
    * @param[in]  code      The code
    * @param[in]  action    The action
    */
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
            eosio::check(has_auth(p2p::_rater) || has_auth(p2p::_me), "missing required authority");
            execute_action(name(receiver), name(code), &p2p::setrate);
          } else if (action == "uprate"_n.value){
            execute_action(name(receiver), name(code), &p2p::uprate);
          } else if (action == "delrate"_n.value){
            execute_action(name(receiver), name(code), &p2p::delrate);
          } else if (action == "setbrate"_n.value){
            execute_action(name(receiver), name(code), &p2p::setbrate);
          } else if (action == "withdrawsh"_n.value){
            execute_action(name(receiver), name(code), &p2p::withdrawsh);
          } else if (action == "refreshsh"_n.value){
            execute_action(name(receiver), name(code), &p2p::refreshsh);
          } else if (action == "delvesting"_n.value){
            execute_action(name(receiver), name(code), &p2p::delvesting);
          } else if (action == "setparams"_n.value){
            execute_action(name(receiver), name(code), &p2p::setparams);
          } else if (action == "fixrate"_n.value){
            execute_action(name(receiver), name(code), &p2p::fixrate);
          } else if (action == "subbbal"_n.value){
            execute_action(name(receiver), name(code), &p2p::subbbal);
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
            
            if (op.to == p2p::_me) {

              eosio::name host = name(op.memo.c_str());
            
              if (host.value == 0) {

                p2p::add_balance(op.from, op.quantity, eosio::name(code));  

              } else {
                p2p::addbbal(host, eosio::name(code), op.quantity);
              }
              
            }
          }
        }
  };
};