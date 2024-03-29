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

/**
 * @brief      Начните ознакомление здесь.
 */
class [[eosio::contract]] p2p : public eosio::contract {

public:
    p2p( eosio::name receiver, eosio::name code, eosio::datastream<const char*> ds ): eosio::contract(receiver, code, ds)
    {}
    void apply(uint64_t receiver, uint64_t code, uint64_t action);
    
    static void add_balance(eosio::name payer, eosio::asset quantity, eosio::name contract);   
    static void sub_balance(eosio::name username, eosio::asset quantity, eosio::name contract);

    static void addbbal(eosio::name host, eosio::name contract, eosio::asset quantity);
    
    static void make_vesting_action(eosio::name owner, eosio::name contract, eosio::asset amount);

    static void check_bonuse_system(eosio::name creator, eosio::name reciever, eosio::asset quantity);

    static void check_guest_and_gift_account(eosio::name username, eosio::name contract, eosio::asset amount, uint64_t gift_account_from_amount);

    static uint64_t get_order_id();

    [[eosio::action]]
    void createorder(name username, uint64_t parent_id, name type, eosio::name root_contract, eosio::asset root_quantity, eosio::name quote_type, double quote_rate, eosio::name quote_contract, eosio::asset quote_quantity, eosio::name out_type, double out_rate, eosio::name out_contract, eosio::asset out_quantity, std::string details);

    [[eosio::action]]
    void accept(name username, uint64_t id, std::string details);

    [[eosio::action]] 
    void fixrate(uint64_t id);

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
    void delvesting(eosio::name owner, uint64_t id);

    [[eosio::action]]
    void setbrate(eosio::name host, double distribution_rate);

    [[eosio::action]] 
    void refreshsh (eosio::name owner, uint64_t id);
    
    [[eosio::action]] 
    void withdrawsh(eosio::name owner, uint64_t id);
    
    [[eosio::action]] 
    void setparams(bool enable_growth, eosio::name growth_type, double start_rate, uint64_t percents_per_month, bool enable_vesting, uint64_t vesting_seconds, eosio::time_point_sec vesting_pause_until, uint64_t gift_account_from_amount, eosio::name ref_pay_type);

    [[eosio::action]] 
    void subbbal(eosio::name host, eosio::name contract, eosio::asset quantity);


    /**
    * @ingroup public_consts 
    * @{ 
    */
    
    static constexpr eosio::name _me = "p2p"_n;                                             /*!< собственное имя аккаунта контракта */
    static constexpr eosio::name _curator = "p2p"_n;                                        /*!< дефолтное имя аккаунта куратора всех сделок */
    static constexpr eosio::name _rater = "rater"_n;                                        /*!< имя аккаунта поставщика курсов */
    static constexpr eosio::symbol _SYM     = eosio::symbol(eosio::symbol_code("FLOWER"), 4);  /*!< системный токен */
    static constexpr eosio::name _core = "unicore"_n;                                       /*!< имя аккаунта распределения реферальных бонусов в сеть */
    static constexpr eosio::name _CORE_SALE_ACCOUNT = "core"_n;                             /*!< аккаунт системного продавца токенов, в сделке к которым срабатывает вестинг */
    static constexpr eosio::name _REGISTRATOR_ACCOUNT = "registrator"_n;                    /*!< аккаунт контракта регистратора, хранящего таблицу с гостями для подарочного выкупа */
    

    static const uint64_t _PERCENTS_PER_MONTH = 42;                                         /*!< если рост курса системного токена подключен, то растёт с указанной здесь скоростью */
    
    static const uint64_t _HUNDR_PERCENT = 100 * 10000;

    static const bool _ENABLE_GROWHT = false;                                                /*!< флаг подключения автоматического роста курса, допускающего вызов метода uprate от системного контракта eosio */

    static const bool _ENABLE_VESTING = false;                                               /*!< флаг подключения режима вестинга для совершенных покупок у аккаунта _CORE_SALE_ACCOUNT */
    static const uint64_t _VESTING_SECONDS = 15770000;                                      /*!< продолжительность вестинга в секундах */
    
    static const uint64_t _GIFT_ACCOUNT_FROM_AMOUNT = 100000;                               /*!< подарок в виде аккаунта партнера осуществляется, если гость совершает покупку на сумму более, чем указано здесь (с учётом точности) */

    // static const uint64_t _ORDER_EXPIRATION = 10;                                        /*!< время до истечения срока давности ордера */
    static const uint64_t _ORDER_EXPIRATION = 30 * 60;                                      /*!< время до истечения срока давности ордера */
    static constexpr double _START_RATE = 0.2;                                              /*!< начальный курс старта роста системного токена относительно USD */

    /**
    * @}
    */

    static uint128_t combine_ids(const uint64_t &x, const uint64_t &y) {
        return (uint128_t{x} << 64) | y;
    };


    /**
     * @brief      Таблица параметров контракта.
     * @ingroup public_tables
     * @table params
     * @contract _me
     * @scope _me
     * @details Таблица хранит параметры управления курсом и заморозками балансов при покупке
     */

    struct [[eosio::table]] params {
        uint64_t id;                    /*!< идентификатор баланса */
        bool enable_growth = false;     /*!< флаг роста */
        eosio::name growth_type;        /*!< тип роста */
        double start_rate = 0.2;        /*!< стартовый курс */
        uint64_t percents_per_month;    /*!< темп роста */
        bool enable_vesting = false;    /*!< флаг заморозки */
        uint64_t vesting_seconds = 0;   /*!< продолжительность заморозки */
        eosio::time_point_sec vesting_pause_until;           /*!< разморозка всех вестингов приостанавливается до даты */ 
        
        uint64_t gift_account_from_amount = 0; /*!< аккаунт в дар от покупки на сумму*/
        eosio::name ref_pay_type;               /*!< тип начисления реферальной выплаты (fixed/flexible) */
        
        uint64_t primary_key() const {return id;} /*!< return id - primary_key */
        
        EOSLIB_SERIALIZE(params, (id)(enable_growth)(growth_type)(start_rate)(percents_per_month)(enable_vesting)(vesting_seconds)(vesting_pause_until)(gift_account_from_amount)(ref_pay_type))
    };


    typedef eosio::multi_index<"params"_n, params> params_index;



    /**
     * @brief      Таблица промежуточного хранения балансов пользователей.
     * @ingroup public_tables
     * @table balance
     * @contract _me
     * @scope username
     * @details Таблица баланса пользователя пополняется им путём совершения перевода на аккаунт контракта p2p. При создании ордера используется баланс пользователя из этой таблицы. Чтобы исключить необходимость пользователю контролировать свой баланс в контракте p2p, терминал доступа вызывает транзакцию с одновременно двумя действиями: перевод на аккаунт p2p и создание ордера на ту же сумму. 
     */

    struct [[eosio::table]] balance {
        uint64_t id;                    /*!< идентификатор баланса */
        eosio::name contract;           /*!< имя контракта токена */
        eosio::asset quantity;          /*!< количество токенов на балансе */

        uint64_t primary_key() const {return id;} /*!< return id - primary_key */
        uint128_t byconsym() const {return combine_ids(contract.value, quantity.symbol.code().raw());} /*!< return combine_ids(contract, quantity) - комбинированный secondary_index 2 */

        EOSLIB_SERIALIZE(balance, (id)(contract)(quantity))
    };

    

    typedef eosio::multi_index<"balance"_n, balance,
    
      eosio::indexed_by< "byconsym"_n, eosio::const_mem_fun<balance, uint128_t, &balance::byconsym>>
    
    > balances_index;



    /**
     * @brief      Таблица счётчиков ордеров
     * @ingroup public_tables
     * @table counts
     * @contract _me
     * @scope _me
     
     * @details Используется для хранения счётчика ордеров с ключом totalorders. При создании нового ордера, счётчик увеличивается на 1. При завершении или удалении ордера, счётчик не изменяется. 
     */
    struct [[eosio::table]] counts {
        name key;           /*!< идентификатор ключа */
        uint64_t value;     /*!< идентификатор значения */

        uint64_t primary_key()const { return key.value; } /*!< return key - primary_key */
        
    };

    typedef eosio::multi_index<"counts"_n, counts> counts_index;
    

    /**
     * @brief      Таблица ордеров
     * @ingroup public_tables
     * @table orders
     * @contract _me
     * @scope _me
     * @details    Ордера создаются продавцами или покупателями вызовом метода createorder, с дальнейшим использованием методов accept, approve и confirm.
    */

    struct [[eosio::table]] orders {
        uint64_t id;                                /*!< идентификатор ордера */
        uint64_t parent_id;                         /*!< идентификатор родительского ордера, с которым происходит сделка */

        name curator;                               /*!< имя аккаунта куратора (оракула), обладающий доступом к методам разрешения конфликтов */
        name creator;                               /*!< имя аккаунта создателя ордера */
        name parent_creator;                        /*!< имя аккаунта создателя родительского ордера */
        name type;                                  /*!< тип ордера buy / sell */
        asset root_quantity;                        /*!< количество токенов на обмене */

        std::string root_symbol;                    /*!< символ токена обмена */
        eosio::name root_contract;                  /*!< контракт токена обмена */
        uint64_t root_precision;                    /*!< точность токена обмена */

        asset root_remain;                          /*!< сколько токенов осталось на обмене */
        eosio::asset root_locked;                   /*!< сколько токенов заблокировано в обмене */
        eosio::asset root_completed;                /*!< сколько токенов завершили обмен */

        name quote_type;                            /*!< маркер внутреннего обмена external / internal. Сейчас используем только external, при internal контракт должен передать out_asset покупателю/продавцу, при external покупатель/продавец должен подтвердить поступление средств на свой счёт вручную, а средства это - внешние относительно блокчейна валюты (фиат). */
        double quote_rate;                          /*!< курс опорной валюты при конвертации (USD) */
        std::string quote_symbol;                   /*!< символ опорной валюты при конвертации (USD) */
        eosio::name quote_contract;                 /*!< контракт опорной валюты при конвертации (при USD - не указывается) */
        uint64_t quote_precision;                   /*!< точность опорной валюты при конвертации (4 знака для USD) */

        asset quote_quantity;                       /*!< общее количество опорной валюты на конвертации (USD) */
        asset quote_remain;                         /*!< сколько опорной валюты осталось на конвертации (USD) */ 
        asset quote_locked;                         /*!< сколько опорной валюты заблокировано на конвертации (USD) */ 
        asset quote_completed;                      /*!< сколько опорной валюты завершило конвертацию (USD) */ 

        uint64_t out_currency_code;                 /*!< код валюты выхода из конвертации по стандарту ISO3166 (не используем сейчас) */ 
        name out_type;                              /*!< тип валюты выхода из конвертации (crypto / fiat) - не используем сейчас */ 
        double out_rate;                            /*!< курс валюты выхода из конвертации относительно опорной валюты (USD) */ 
        std::string out_symbol;                     /*!< символ валюты выхода из конвертации */ 
        name out_contract;                          /*!< контракт валюты выхода из конвертации (не используется сейчас) */ 
        uint64_t out_precision;                     /*!< точность валюты выхода из конвертации */ 
        
        asset out_quantity;                         /*!< общее количество валюты выхода из конвертации */ 
        asset out_remain;                           /*!< сколько валюты выхода осталось на конвертации */ 
        asset out_locked;                           /*!< сколько валюты выхода заблокировано на конвертации */ 
        asset out_completed;                        /*!< сколько валюты выхода завершило конвертацию */ 

        std::string details;                        /*!< обычно зашированные детали сделки */ 
        name status;                                /*!< статус ордера: waiting / process / payed / finish / dispute */ 
        eosio::time_point_sec expired_at;           /*!< дата истечения срока давности ордера */ 
        eosio::time_point_sec created_at;           /*!< дата создания ордера */ 

        uint64_t primary_key()const { return id; }                              /*!< return id - primary_key */
        uint64_t bycurrcode() const {return out_currency_code;}                 /*!< out_currency_code - secondary_key 2 */

        uint64_t byparentid()const { return parent_id;}                         /*!< parent_id - secondary_key 3 */
        uint64_t bytype() const {return type.value;}                            /*!< type - secondary_key 4 */

        uint64_t bycreator() const {return creator.value;}                      /*!< creator - secondary_key 5 */
        uint64_t bycurator() const {return curator.value;}                      /*!< curator - secondary_key 6 */
        uint64_t bystatus() const {return status.value;}                        /*!< status - secondary_key 7 */
        uint64_t byexpr() const {return expired_at.sec_since_epoch();}          /*!< expired_at - secondary_key 8 */
        uint64_t bycreated() const {return created_at.sec_since_epoch();}       /*!< created_at - secondary_key 9 */
        
        uint128_t byparandus() const {return combine_ids(parent_id, creator.value);} /*!< (out_contract, out_asset.symbol) - комбинированный secondary_key для получения курса по имени выходного контракта и символу */
        
    };

    typedef eosio::multi_index< "orders"_n, orders,
        eosio::indexed_by<"bycurrcode"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycurrcode>>,
        eosio::indexed_by<"byparentid"_n, eosio::const_mem_fun<orders, uint64_t, &orders::byparentid>>,
        eosio::indexed_by<"bytype"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bytype>>,
        eosio::indexed_by<"bycreator"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycreator>>,
        eosio::indexed_by<"bycurator"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycurator>>,
        eosio::indexed_by<"bystatus"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bystatus>>,
        eosio::indexed_by<"byexpr"_n, eosio::const_mem_fun<orders, uint64_t, &orders::byexpr>>,
        eosio::indexed_by<"bycreated"_n, eosio::const_mem_fun<orders, uint64_t, &orders::bycreated>>,
        eosio::indexed_by<"byparandus"_n, eosio::const_mem_fun<orders, uint128_t, &orders::byparandus>>
  
    > orders_index;



    /**
     * @brief      Таблица содержит курсы конвертации к доллару.
     * @ingroup public_tables
     * @table usdrates
     * @contract _me
     * @scope _me
     * @details    Курсы обновляются аккаунтом rater методом setrate или системным контрактом eosio методом uprate. 
    */
    
    struct [[eosio::table]] usdrates {
        uint64_t id;                        /*!< идентификатор курса */
        eosio::name out_contract;           /*!< контракт выхода; если в конвертации используется внешняя валюта (например, фиатный RUB), контракт не устанавливается. Во внутренних конвертациях используется только при указании курса жетона ядра системы к доллару. */
        eosio::asset out_asset;             /*!< токен выхода */
        double rate;                        /*!< курс токена выхода к доллару */
        eosio::time_point_sec updated_at;   /*!< дата последнего обновления курса */
        
        uint64_t primary_key() const {return id;} /*!< return id - primary_key */
        uint128_t byconsym() const {return combine_ids(out_contract.value, out_asset.symbol.code().raw());} /*!< (out_contract, out_asset.symbol) - комбинированный secondary_key для получения курса по имени выходного контракта и символу */

    };

    typedef eosio::multi_index<"usdrates"_n, usdrates,
    
      eosio::indexed_by< "byconsym"_n, eosio::const_mem_fun<usdrates, uint128_t, &usdrates::byconsym>>
    
    > usdrates_index;



    /**
     * @brief      Таблица расширения usdrates с указанием даты установки первого курса
     * @ingroup public_tables
     * @table usdrates2
     * @contract _me
     * @scope _me
     
     */
    struct [[eosio::table]] usdrates2 {
        uint64_t id; /*!< идентификатор курса */
        eosio::time_point_sec created_at; /*!< дата установки первого курса */
        
        uint64_t primary_key() const {return id;} /*!< return id - primary_key */
    };


    typedef eosio::multi_index<"usdrates2"_n, usdrates2> usdrates2_index;


    /**
     * @brief      Таблица резервов контракта для выплат бонусов в реферальную сеть
     * @ingroup public_tables
     * @table bbonuses
     * @contract _me
     * @scope _me
     * @details    Таблица пополняется переводом на аккаунт контракта с указаним в поле memo аккаунта продавца, 
     * который будет использовать распределение на сеть покупателя. Распределение срабатывает в момент завершения сделки, увеличивая значение в поле distributed согласно курсу распределения disctribution_rate.
     */
    struct [[eosio::table]] bbonuses {
        eosio::name host;           /*!< имя хоста продавца, при сделке с которым, срабатывает выплата в сеть */
        eosio::name contract;       /*!< имя контракта токена */
        eosio::asset total;         /*!< сколько токенов всего было на распределении */
        eosio::asset available;     /*!< сколько токенов доступно для распределения */
        eosio::asset distributed;   /*!< сколько токенов уже распределено */
        double distribution_rate;   /*!< курс распределения бонусов, согласно которому, в сеть распределяется столько же монет, сколько получил пользователь, умноженное на этот курс */

        uint64_t primary_key() const {return host.value;} /*!< return host - primary_key */
    };


    typedef eosio::multi_index<"bbonuses"_n, bbonuses> bbonuses_index;




    /**
     * @brief      Таблица вестинг-балансов пользователей
     * @ingroup public_tables
     * @table vesting
     * @contract _me
     * @scope owner
     * @details Пополняется контрактом в случае, если ключ _ENABLE_VESTING = TRUE на количество секунд в _VESTING_SECONDS, срабатывает только для аккаунта продавца _CORE_SALE_ACCOUNT. Позволяет заморозить покупку токенов у компании на указанное количество секунд.
     */
      struct [[eosio::table]] vesting {
        uint64_t id;                            /*!< идентификатор объекта вестинга */
        eosio::name owner;                      /*!< имя аккаунта владельца объекта вестинга (дублируется со SCOPE) */
        eosio::name contract;                   /*!< имя контракта токена вестинга */
        eosio::time_point_sec startat;          /*!< объект вестинга создан в */
        uint64_t duration;                      /*!< продолжительность вестинга в секундах */
        eosio::asset amount;                    /*!< общее количество токенов на ветсинге */
        eosio::asset available;                 /*!< доступное количество токенов из вестинга */
        eosio::asset withdrawed;                /*!< выведенное количество токенов из вестинга */

        uint64_t primary_key() const {return id;} /*!< return id - primary_key */

        EOSLIB_SERIALIZE(vesting, (id)(owner)(contract)(startat)(duration)(amount)(available)(withdrawed))
      };

      typedef eosio::multi_index<"vesting"_n, vesting> vesting_index;



    /**
     * @brief      Таблица доступа к записям гостей платформы
     * @ingroup public_tables
     * @table guests
     * @contract _REGISTRATOR_ACCOUNT
     * @scope _REGISTRATOR_ACCOUNT
     
     * @details Таблица находится на контракте registrator и используется для проверки необходимости выкупа аккаунта пользователя, если его покупка у _CORE_SALE_ACCOUNT больше, чем _GIFT_ACCOUNT_FROM_AMOUNT. Если пользователю полагается подарочный аккаунт, то контракт p2p совершает его выкуп для пользователя из числа токенов бонусного баланса контракта.
     */
    
    struct [[eosio::table]] guests {
        eosio::name username;             /*!< имя аккаунта гостя */
        
        eosio::name registrator;          /*!< имя аккаунта регистратора гостя */
        eosio::public_key public_key;     /*!< публичный ключ гостя, который использовался в качестве активного ключа и на который будут переданы права владельца после оплаты */
        eosio::asset cpu;                 /*!< количество системного токена, закладываемого в CPU */
        eosio::asset net;                 /*!< количество системного токена, закладываемого в NET */
        bool set_referer = false;         /*!< флаг автоматической регистрации партёром (не используется) */
        eosio::time_point_sec expiration; /*!< дата истечения пользования аккаунтом */

        eosio::asset to_pay;              /*!< количество токенов к оплате */
        
        uint64_t primary_key() const {return username.value;}     /*!< return username - primary_key */
        uint64_t byexpr() const {return expiration.sec_since_epoch();} /*!< return expiration - secondary_key 2 */
        uint64_t byreg() const {return registrator.value;}            /*!< return registrator - secondary_key 3 */

        EOSLIB_SERIALIZE(guests, (username)(registrator)(public_key)(cpu)(net)(set_referer)(expiration)(to_pay))
    };


    typedef eosio::multi_index<"guests"_n, guests,
       eosio::indexed_by< "byexpr"_n, eosio::const_mem_fun<guests, uint64_t, &guests::byexpr>>,
       eosio::indexed_by< "byreg"_n, eosio::const_mem_fun<guests, uint64_t, &guests::byreg>>
    > guests_index;




/*!
   \brief Структура хоста Двойной Спирали.
*/

    struct [[eosio::table, eosio::contract("unicore")]] hosts{
        eosio::name username;
        eosio::time_point_sec registered_at;
        eosio::name architect;
        eosio::name hoperator;
        eosio::name type;
        eosio::name chat_mode;
        uint64_t consensus_percent;
        uint64_t referral_percent;
        uint64_t dacs_percent;
        uint64_t cfund_percent;
        uint64_t hfund_percent;
        uint64_t sys_percent;
        std::vector<uint64_t> levels;
        std::vector<eosio::name> gsponsor_model;
        bool direct_goal_withdraw = false;
        uint64_t dac_mode;
        uint64_t total_dacs_weight;
        
        eosio::name ahost;
        std::vector<eosio::name> chosts;
        
        bool sale_is_enabled = false;
        eosio::name sale_mode;
        eosio::name sale_token_contract;
        eosio::asset asset_on_sale;
        uint64_t asset_on_sale_precision;
        std::string asset_on_sale_symbol;
        int64_t sale_shift = 1;
        
        bool non_active_chost = false;
        bool need_switch = false;

        uint64_t fhosts_mode;
        std::vector<eosio::name> fhosts;
        

        std::string title;
        std::string purpose;
        bool voting_only_up = false;
        eosio::name power_market_id;
        uint64_t total_shares;
        eosio::asset quote_amount;
        eosio::name quote_token_contract;
        std::string quote_symbol;
        uint64_t quote_precision;
        eosio::name root_token_contract;
        eosio::asset root_token;
        std::string symbol;
        uint64_t precision;
        eosio::asset to_pay;
        bool payed = false;
        
        uint64_t cycle_start_id = 0;
        uint64_t current_pool_id = 0;
        uint64_t current_cycle_num = 1;
        uint64_t current_pool_num = 1;
        bool parameters_setted = false;
        bool activated = false;
        
        bool priority_flag = false;

        uint64_t total_goals = 0;
        uint64_t achieved_goals = 0;
        uint64_t total_tasks = 0;
        uint64_t completed_tasks = 0;
        uint64_t total_reports = 0;
        uint64_t approved_reports = 0;
        

        std::string meta;
        


        EOSLIB_SERIALIZE( hosts, (username)(registered_at)(architect)(hoperator)(type)(chat_mode)(consensus_percent)(referral_percent)
            (dacs_percent)(cfund_percent)(hfund_percent)(sys_percent)(levels)(gsponsor_model)(direct_goal_withdraw)(dac_mode)(total_dacs_weight)(ahost)(chosts)
            (sale_is_enabled)(sale_mode)(sale_token_contract)(asset_on_sale)(asset_on_sale_precision)(asset_on_sale_symbol)(sale_shift)
            (non_active_chost)(need_switch)(fhosts_mode)(fhosts)
            (title)(purpose)(voting_only_up)(power_market_id)(total_shares)(quote_amount)(quote_token_contract)(quote_symbol)(quote_precision)(root_token_contract)(root_token)(symbol)(precision)
            (to_pay)(payed)(cycle_start_id)(current_pool_id)
            (current_cycle_num)(current_pool_num)(parameters_setted)(activated)(priority_flag)(total_goals)(achieved_goals)(total_tasks)(completed_tasks)(total_reports)(approved_reports)(meta))

        uint64_t primary_key()const { return username.value; }

        eosio::name get_ahost() const {
            if (ahost == username)
                return username;
            else 
                return ahost; 
        }

        eosio::symbol get_root_symbol() const {
            return root_token.symbol;
        }


    };

    typedef eosio::multi_index <"hosts"_n, hosts> account_index;
    
};
