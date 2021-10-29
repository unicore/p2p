#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/contract.hpp>
#include <eosio/crypto.hpp>

#include <algorithm>
#include <cmath>

using namespace eosio;

class [[eosio::contract]] p2p : public eosio::contract {

public:
    p2p( eosio::name receiver, eosio::name code, eosio::datastream<const char*> ds ): eosio::contract(receiver, code, ds)
    {}
    void apply(uint64_t receiver, uint64_t code, uint64_t action);
    
    static void add_balance(eosio::name payer, eosio::asset quantity, eosio::name contract);   
    static void sub_balance(eosio::name username, eosio::asset quantity, eosio::name contract);

    static void addbbal(eosio::name host, eosio::name contract, eosio::asset quantity);
    static void subbbal(eosio::name host, eosio::name contract, eosio::asset quantity);


    static void check_bonuse_system(eosio::name creator, eosio::asset quantity);

    static uint64_t get_order_id();

    [[eosio::action]]
    void createorder(name username, uint64_t parent_id, name type, eosio::name root_contract, eosio::asset root_quantity, eosio::name quote_type, double quote_rate, eosio::name quote_contract, eosio::asset quote_quantity, eosio::name out_type, double out_rate, eosio::name out_contract, eosio::asset out_quantity, std::string details);

    [[eosio::action]]
    void accept(name username, uint64_t id, std::string details);
     
    [[eosio::action]]
    void approve(name username, uint64_t id);

    [[eosio::action]]
    void confirm(name username, uint64_t id);

    [[eosio::action]]
    void cancel(name username, uint64_t id);
    
    [[eosio::action]]
    void dispute(name username, uint64_t id);
    
    [[eosio::action]]
    void del(name username, uint64_t id);
    
    [[eosio::action]]
    void setrate(eosio::name out_contract, eosio::asset out_asset, double rate);
    
    [[eosio::action]]
    void uprate(eosio::name out_contract, eosio::asset out_asset);
     
    [[eosio::action]]
    void delrate(uint64_t id);
    
    [[eosio::action]]
    void setbrate(eosio::name host, double distribution_rate);
    

    
    
    
    static constexpr eosio::name _me = "p2p"_n;
    static constexpr eosio::name _curator = "p2p"_n;
    static constexpr eosio::name _rater = "rater"_n;
    static constexpr eosio::name _core = "unicore"_n;
    
    static constexpr eosio::symbol _SYM     = eosio::symbol(eosio::symbol_code("FLOWER"), 4);
    static const uint64_t _PERCENTS_PER_MONTH = 10;

    static const bool _ENABLE_GROWHT = true;

    // static const uint64_t _ORDER_EXPIRATION = 10; //10 secs
    static const uint64_t _ORDER_EXPIRATION = 30 * 60; //30 mins
    static constexpr double _START_RATE = 0.5;

    struct [[eosio::table]] balance {
        uint64_t id;
        eosio::name contract;
        eosio::asset quantity;

        uint64_t primary_key() const {return id;}
        uint128_t byconsym() const {return combine_ids(contract.value, quantity.symbol.code().raw());}

        EOSLIB_SERIALIZE(balance, (id)(contract)(quantity))
    };

    

    typedef eosio::multi_index<"balance"_n, balance,
    
      eosio::indexed_by< "byconsym"_n, eosio::const_mem_fun<balance, uint128_t, &balance::byconsym>>
    
    > balances_index;


    static uint128_t combine_ids(const uint64_t &x, const uint64_t &y) {
        return (uint128_t{x} << 64) | y;
    };


    struct [[eosio::table]] counts {
        name key;
        uint64_t value;

        uint64_t primary_key()const { return key.value; }
        
    };

    typedef eosio::multi_index<"counts"_n, counts> counts_index;
    

    //что сделать? 

    struct [[eosio::table]] orders {
        uint64_t id;
        uint64_t parent_id;

        name curator;
        name creator;
        name parent_creator;
        name type;
        asset root_quantity;

        std::string root_symbol;
        eosio::name root_contract; 
        uint64_t root_precision;

        asset root_remain;
        eosio::asset root_locked;
        eosio::asset root_completed;

        name quote_type;
        double quote_rate;
        std::string quote_symbol;
        eosio::name quote_contract;
        uint64_t quote_precision;

        asset quote_quantity;
        asset quote_remain;
        asset quote_locked;
        asset quote_completed;

        uint64_t out_currency_code; //ISO3166
        name out_type;
        double out_rate;
        std::string out_symbol;
        name out_contract;
        uint64_t out_precision;
        
        asset out_quantity;
        asset out_remain;
        asset out_locked;
        asset out_completed;

        std::string details;
        name status;
        eosio::time_point_sec expired_at;
        eosio::time_point_sec created_at;

        uint64_t primary_key()const { return id; }
        uint64_t bycurrcode() const {return out_currency_code;}

        uint64_t byparentid()const { return parent_id;}
        uint64_t bytype() const {return type.value;} 

        uint64_t bycreator() const {return creator.value;}
        uint64_t bycurator() const {return curator.value;}
        uint64_t bystatus() const {return status.value;} 
        uint64_t byexpr() const {return expired_at.sec_since_epoch();}
        uint64_t bycreated() const {return created_at.sec_since_epoch();}
    
    };

    typedef eosio::multi_index< "orders"_n, orders,
        eosio::indexed_by<"bycurrcode"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycurrcode>>,
        eosio::indexed_by<"byparentid"_n, eosio::const_mem_fun<orders, uint64_t, &orders::byparentid>>,
        eosio::indexed_by<"bytype"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bytype>>,
        eosio::indexed_by<"bycreator"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycreator>>,
        eosio::indexed_by<"bycurator"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycurator>>,
        eosio::indexed_by<"bystatus"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bystatus>>,
        eosio::indexed_by<"byexpr"_n, eosio::const_mem_fun<orders, uint64_t, &orders::byexpr>>,
        eosio::indexed_by<"bycreated"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycreated>>
  
    > orders_index;



    struct [[eosio::table]] usdrates {
        uint64_t id;
        eosio::name out_contract;
        eosio::asset out_asset;
        double rate;
        eosio::time_point_sec updated_at;
        
        uint64_t primary_key() const {return id;}
        uint128_t byconsym() const {return combine_ids(out_contract.value, out_asset.symbol.code().raw());}

    };




    typedef eosio::multi_index<"usdrates"_n, usdrates,
    
      eosio::indexed_by< "byconsym"_n, eosio::const_mem_fun<usdrates, uint128_t, &usdrates::byconsym>>
    
    > usdrates_index;



    struct [[eosio::table]] usdrates2 {
        uint64_t id;
        eosio::time_point_sec created_at;
        
        uint64_t primary_key() const {return id;}
    };


    typedef eosio::multi_index<"usdrates2"_n, usdrates2> usdrates2_index;



    struct [[eosio::table]] bbonuses {
        eosio::name host;
        eosio::name contract;
        eosio::asset total;
        eosio::asset available;
        eosio::asset distributed;
        double distribution_rate;

        uint64_t primary_key() const {return host.value;}
    };


    typedef eosio::multi_index<"bbonuses"_n, bbonuses> bbonuses_index;

};
