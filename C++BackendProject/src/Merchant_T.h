#ifndef MERCHANT_H
#define MERCHANT_H

#include <map>
#include <vector>
#include "types.h"

//forward declarations.
class Sort_T;
class DB_T; 

enum Merchant_Status
{
  MERCHANT_STATUS_HIDE   =  1,
  MERCHANT_STATUS_SHOW   =  2,
  //MERCHANT_STATUS_THREE  =  3
};

enum Merchant_ViewMode
{
    MERCHANT_VIEW_NONE       = 0,
    MERCHANT_VIEW_CLIENT     = 1,
    MERCHANT_VIEW_BUSINESS   = 2,
};

//@note  see  merchant_api table  api_id  row.
enum class merchant_api_id
{
    paynet            = 1 , 
    munis             = 2 ,
    hermes_garant     = 3 ,
    mplat             = 4 ,
    oson_self         = 5 ,
    nonbilling        = 6 ,
    uzinfocom         = 7 ,
    comnet            = 8 ,
    sharq_telecom     = 9 ,
    uzonline          = 10,
    beeline           = 11,
    sarkor_telecom    = 12,
    kafolat_insurance = 13,
    nano_telecom      = 14,
    webmoney          = 15,
    ucell             = 16,
    tps               = 17,
    qiwi              = 18,
    ums               = 19,
    uzmobile          = 20,
    uzmobile_old      = 21,
};

inline bool operator == (  merchant_api_id  api_id, int32_t int_val){
    return int_val == static_cast<int32_t>( api_id);
}
inline bool operator == (int32_t int_val, merchant_api_id api_id){
    return int_val == static_cast<int32_t>(api_id);
}


struct merchant_identifiers
{
    enum values
    {
        ISTV_TV         = 1,
        TPS_I           = 6,  TPS_I_direct = 415,
        Beeline         = 15, Beeline_test_dont_use=  424,
        Ucell           = 16, Ucell_direct = 392,
        UMS             = 17, UMS_direct  = 510,
        Perfectum       = 18,
        UzMobile_CDMA   = 19,
        Cron_Telecom    = 21,
        ISTV_I          = 23,
        Ars_Inform      = 26,
        Sarkor_Telecom  = 27,  Sarkor_TV = 386, Sarkor_IPCAM = 387 , Sarkor_HastimUz = 388,
        UzDigital_TV    = 28,
        Sharqona_Lotto  = 30,
        Allmovies_uz    = 31,
        Oxo_Media       = 32,
        Allplay_uz      = 33,
        Nano_Telecom    = 34, Nano_Telecom_direct = 401,
        Net_City        = 35,
        Simus_I         = 36,
        EVO_LTE         = 37,
        Comnet_I        = 38,
        FiberNet_I      = 39,
        Free_Link       = 40,
        Starts_TV       = 41,
        Sonet           = 42,
        i_TV            = 43,
        Tasx_uz         = 44,
        iTest_uz        = 45,
        Smart_Card      = 46,
        NBP_uz          = 47,
        My_Taxi         = 49,
        Antivirusi      = 50,
        Iqtidor_uz      = 51, 
        Cherry_uz       = 52,
        _100_uslug_uz   = 53,
        Namangan_soft_dev = 54,
        Schoolface_uz   = 57,
        Tom_uz          =58,
        Movi_uz         = 59,
        Nnm_uz          = 61,
        Cobo_uz         = 62,
        
        Uzbazar_uz      = 63,
        Dump_uz         = 65,
        Uybor_uz        = 66,
        Dastur_uz       = 67,
        Comza_Cyfral_Service = 68,
        Buzton         = 69,
        Skyline        = 70,
        Baraca_card    = 71,
        Cmaxx_uz       = 72,
        Mnogo_uz       = 74,
        UzMobile_GSM   = 76,
        
        NETCO         = 77,
        NETCO_TV      = 78,
        Evrika_uz     = 79,
        Kitobxon_com   = 80,
        Uchenik_uz     = 81,
        Olx_uz         = 82,
        Zor_uz         = 83,
        Addirect_uz    = 85,
        
        Kitobim_uz     = 86,
        Kinopro_uz     = 87,
        Online_tv_uz   = 88,
        
        Platinum_connect = 91,
        Platinum_mobile = 92,
        
        East_Telecom   = 93, East_Telecom_direct = 400,
        Mediabay_uz    = 94,
        
        Spectr_IT     = 95,
        Bizplan_uz    = 96,
        
        
        Sharq_Telecom  = 98,
        Uzonline_I     = 99,
        
        Rocket_uz      = 100,
        Prep_uz        = 101,
        Mykupon_uz     = 102,
        
        
        GO_TAXI        = 103,
        Ahost_uz       = 104,
        
        JetNet         = 105,
        Bestest_uz     = 106,
        
        Odnoklassniki  = 107,
        Filecloud_uz   = 108,
        Station_uz     = 109,
        Gosuslugi_uz   = 110,
        Tak_uz         = 111,
        Avtobor_uz     = 112,
        Irr_uz         = 113,
        
        
        Uz_registration = 114,
        Omad_Lotto      = 115,
        Abitur_uz       = 116,
        Audiobook_uz    = 117,
        Aore_uz         = 118,
        
        
        Paspviza_uz    = 119,
        Beeline_Internet = 120, Beeline_Internet_test = 396,
        Beeline_IP_telephone = 121,
        
        GTS_g_TASHKENT   = 127,
        GTS_Bukhara      = 129,
        GTS_Andijan      = 130,
        GTS_obl_Djizzak  = 131,
        GTS_g_Djizzak    = 133,
        GTS_Qashqadarya  = 134,
        GTS_Navoi        = 135,
        GTS_g_Samarkand  = 136,
        GTS_obl_Samarkand = 137,
        GTS_Surxandarya  = 138,
        GTS_Sirdarya     = 139,
        GTS_Namangan     = 140,
        GTS_obl_TASHKENT = 141,
        GTS_Fergana      = 142,
        GTS_Qoraqalpoq   = 143,
        GTS_Xorezm       = 144,
        
        Electro_Energy = 146,
        Uzinfocom      = 147,
        BeautyBox      = 148,
        Musor          = 152,
        
        Munis_GUBDD    = 158,
        Munis_KinderGarden = 515,
        
        
        Gross_insurance = 159,
        /**********************/
        Mplat_QiwiUSD                = 182,
        Mplat_QiwiRUB                = 183,
        Mplat_Warface                = 184,
        Mplat_WebMoneyRUB            = 185,
        Mplat_WebMoneyUSD            = 186,
        Mplat_WebMoneyEUR            = 187,
        Mplat_WalletOneUSD           = 188,
        Mplat_YandexMoney            = 189,
        Mplat_Steam                  = 190,
        Mplat_WarTank                = 191,
        Mplat_WarThunder             = 192,
        Mplat_Moneta_ru              = 193,
        Mplat_WalletOneRUB           = 194,
        Mplat_QiwiWalletEUR          = 195,
        Mplat_KCELL                  = 196,
        Mplat_Activ                  = 197,
        Mplat_Tele2                  = 198,
        Mplat_Beeline_Russia         = 199,
        Mplat_Tele2_Russia           = 200,
        MPlat_MTC_Russia             = 201,
        Mplat_Beeline_Khazaxstan     = 202,
        Mplat_4Game                  = 203,
        Mplat_Megafon_Moscow         = 204,
        Mplat_Art_of_War2            = 205,
        Mplat_Vichatter              = 206,
        Mplat_BabilonM_Tadjikistan   = 207,
        Mplat_World_of_Tanks_LG      = 208,
        Mplat_odnoklassniki_WS       = 209,
        Mplat_Vkontakte_ReklamaWS    = 210,
        Mplat_Vkontakte_GolosWS      = 211,
        Mplat_Moy_Mir                = 212,
        
        ///////////////////////////////////////
        Technomart_St_Kichik_Xalqa  = 218,
        Technomart_St_Farxadskiy    = 220,
        Technomart_St_Almazar       = 221,
        Technomart_St_Avliyo_Ota    = 222,
        Technomart_St_Chilanzar     = 223,
        Technomart_St_Beshariq      = 224,
        Technomart_St_Amir_Temur    = 225,
        Technomart_St_Parkentskiy   = 226,
        
        ////////////////////////////////////////
        Mplat_Tanki_online           = 230,
        Mplat_paypal                 = 231,
        Mplat_wiber                  = 232,
        MPlat_3kkk                   = 233,
        Mplat_Legenda                = 234,
        Mplat_PerfectWorld           = 235,
        Mplat_Revelation             = 236,
        Mplat_BloodyWorld            = 237,
        Mplat_TimeZero               = 238,
        Mplat_JadeDynasty            = 239,
        Mplat_Bumz                   = 240,
        Mplat_GroundWar_Tanks        = 241,
        Mplat_DragonNest             = 242,
        Mplat_Djagernaunt            = 243,
        Mplat_VoynaPrestolov         = 244,
        Mplat_SpartaVoynaImperiy     = 245,
        Mplat_ParaPa_GorodTansov     = 246,
        Mplat_KodeksPirata           = 247,
        Mplat_MiniIgriMail_ru        = 248,
        Mplat_RIOT                   = 249,
        Mplat_SkyForge               = 250,
        Mplat_CrossFire              = 251,
        Mplat_ArmoredWarFire_Armata  = 252,
        Mplat_Allodi_Online          = 253,
        Mplat_ArchAge                = 254,
        Mplat_Xcraft                 = 255,

        Mplat_Game_Xsolla       = 267 , 
        Mplat_Game_UFO_Online       = 268 , //"UFO Online","268"
        Mplat_Game_Grani_Real       = 269 , //"Грани Реальности","269"
        Mplat_Game_GameNet          = 270 , //"GameNet","270"
        Mplat_Game_Unit             = 271 , //"Unit ","271"
        Mplat_Game_Atom_Fishing_Ex  = 272 , //"Atom Fishing Экстрим","272"
        Mplat_Game_Sadovniki        = 273 ,   //"Садовники","273"
        Mplat_Game_Mars             = 274 ,   //"Марс","274"
        Mplat_Game_Arena_ru         = 275 ,   //"arena.ru","275"
        Mplat_Game_IQsha_ru         = 276 ,   //"IQsha.ru","276"
        Mplat_Game_Neverlands       = 277 ,  //"Neverlands ","277"
        Mplat_Game_Encounter        = 278 ,  //"Encounter ","278"
        Mplat_Game_Battle_Carnival  = 279 , //"Battle Carnival","279"
        Mplat_Game_Black_Desert     = 280 , //"Black Desert","280"
        Mplat_Game_Crossout         = 281 , //"Crossout ","281"
        Mplat_Game_Combat_Arms      = 282 , //"Combat Arms","282"
        Mplat_Game_BS_RU            = 283 , //"BS.RU","283"
        Mplat_Game_Bashni           = 284 , //"Башни","284"
        Mplat_Game_Aviatori         = 285 , //"Авиаторы ","285"
        Mplat_Game_Bitva_magov      = 286 , //"Битва магов","286"
        Mplat_Game_Volshebniki      = 287 , //"Волшебники ","287"
        Mplat_Game_Poviliteli_stixiy = 288, //"Повелители стихий","288"
        Mplat_Game_Era_Herous       = 289 , //"Эра героев","289"
        Mplat_Game_Max_Speed        = 290 , //"Максимальная скорость","290"
        Mplat_Game_Drakoni          = 291 , //"Драконы ","291"
        Mplat_Game_Puti_Istoriy     = 292 , //"Пути истории","292"
        Mplat_Game_Zamki            = 293 , //"Замки ","293"
        Mplat_Game_Chat_realiti     = 294 , //"Чат реалити","294"
        Mplat_Game_Imperiy          = 295 , //"Имперцы ","295"
        Mplat_Game_Ostrova          = 296 , //"Острова ","296"
        Mplat_Game_Groza_morey      = 294 , //"Гроза морей","297"
        Mplat_Game_Virtual_Futbol_Liga = 298, //"Виртуальная Футбольная Лига","298"
        Mplat_Game_Zvezniy_Boi      = 299 , //"Звездные Бои","299"
        Mplat_Game_Generali         = 300 , //"Генералы ","300"
        Mplat_Game_Vgorode          = 301 , //"Вгороде ","301"
        Mplat_Game_Berserk          = 302 , //"Берсерк: Возрождение","302"
        Mplat_Game_Varvari          = 303 , //"Варвары","303"
        Mplat_Game_Nasledi_drev     = 304, //"Наследие древних","304"
        Mplat_Game_XNOVA_2_0        = 305, //"XNOVA 2.0","305"
        Mplat_Game_Berserk_Universe = 306, //"Berserk Universe","306"
        Mplat_Game_Pobiditeli       = 307, //"Победители","307"
        Mplat_Game_Berserk_Online   = 308, //"Берсерк Онлайн","308"
        Mplat_Game_Nayemniki        = 309, //"Наемники","309"
        Mplat_Game_Razrushetili     = 310, //"Разрушители ","310"
        Mplat_Game_Bitva_fermers    = 311, //"Битва фермеров","311"
        Mplat_Game_Super_Gonki      = 312, //"Супер гонки","312"
        Mplat_Game_Bitva_Titanov    = 313, //"Битва титанов","313"
        Mplat_Game_Nebiskrebi       = 314, //"Небоскребы ","314"
        Mplat_Game_Lost_Magic       = 315, //"Lost Magic","315"
        Mplat_Game_Udivitelniy_pitoms = 316, //"Удивительные питомцы","316"
        Mplat_Game_Erudit             = 317, //"Эрудит ","317"
        Mplat_Game_PumpIT             = 318, //"PumpIT","318"
        Mplat_Game_GameMiner          = 319, //"GameMiner ","319"
        Mplat_Game_Moya_Sobaka        = 320, //"Моя собака","320"
        Mplat_Game_Liga_Geroyev       = 321, //"Лига героев","321"
        Mplat_Game_Fantastic_Fishing  = 322,//"Fantastic Fishing","322"
        Mplat_Game_Moya_Ferma         = 323,//"Моя ферма","323"
        Mplat_Game_Vikingi            = 324, //"Викинги ","324"
        Mplat_Game_Tyurma_New_list    = 325,//"Тюрьма. Новый срок","325"
        Mplat_Game_Epoxa_xaosa   = 326,//"Эпоха хаоса","326"
        Mplat_Game_Rashki        = 327,//"Рашка ","327"

        Mplat_Game_Poviliteli    = 328,//"Повелители","328"
        Mplat_Game_One_World     = 329,//"One World","329"
        Mplat_Game_Voyni_mafii   = 330,//"Войны мафии","330"
        Mplat_Game_Sadovaya_imperiy = 331, //"Садовая империя","331"
        Mplat_Game_Moya_Derevnya =  332,//"Моя деревня","332"
        Mplat_Game_Z_WAR         = 333,//"Z-WAR","333"
        Mplat_Game_Torment       = 334, //"Torment","334"
        Mplat_Game_Voyna_Angeli_Demoni = 335,//"Война: Ангелы и демоны","335"
        Mplat_Game_Bratki        = 336, //"Братки","336"
        Mplat_Game_Godvill       = 337,//"Годвилль","337"
        Mplat_Mir_teney          = 338,//"Мир теней","338"
        Mplat_Game_Kalonizatori  = 339, //"Колонизаторы","339"
        Mplat_Game_Parograd      = 340, //"Пароград","340"
        Mplag_Game_Dosenti       = 341, //"Доценты ","341"
        Mplat_Game_Nasledi_xaosa = 342, //"Наследие хаоса","342"
        Mplat_Game_101XP             = 343,//"101XP","343"
        Mplat_Game_Geroi_voyni_deneg = 344,//"Герои войны и денег","344"
        Mplat_Game_Texnomagiya   = 345, //"Техномагия","345"
        Mplat_Game_My_Lands_Black_Gem_Hunting = 346,//"My Lands: Black Gem Hunting","346"
        Mplat_Game_LAVA_online   = 347, //"LAVA online","347"

        Mplat_Game_11x11         = 348, //"11x11","348"
        Mplat_Game_Magic_Zemli   = 349, //"Магические Земли","349"
        Mplat_Game_BananaWars    = 350, //"BananaWars","350"
        Mplat_Game_Peredovaya    = 351, //"Передовая ","351"
        Mplat_Game_Sky2Flay      = 352, //"Sky2Fly","352"
        Mplat_Game_MGates        = 353, //"MGates","353"
        Mplat_Game_Udivitelniy_kolxoz = 354,//"Удивительный колхоз","354"
        Mplat_Game_Ganja_Wars    = 355, //"Ganja Wars","355"
        Mplat_Game_Gladiatori    = 356, //"Гладиаторы","356"
        Mplat_Game_Geroi         = 357, //"Герои ","357"
        Mplat_Game_Star_Conflict = 358, //"Star Conflict","358"
        Mplat_Game_Panzar        = 359, //"Panzar ","359"
        Mplat_Game_Ice_Kings     = 360, //"Ice Kings","360"
        Mplat_Game_STALKER       = 361, //"STALKER","361"
        Mplat_Game_Royal_Quest   = 363, //"Royal Quest [RUS SERVER]","363"


        Mplat_Mamba                  = 364,


        Mplat_Game_Stalnoy_legion   = 366, //  Стальной легион  Игры
        Mplat_Game_Affected_Zone_Tactics  = 367, //  Affected Zone Tactics  
        Mplat_Game_Total_influence_Online = 368, //  Total Influence Online
        Mplat_Game_8day             =  369, //  8day
        Mplat_Game_Voin_Dorogi      =  370, //  Воин Дороги
        Mplat_Game_Monstri          =  371, //  Монстры
        Mplat_Game_Modnisi          =  372, //  Модницы
        Mplat_Game_Virtual_Russian  =  373, //  Виртуальная Россия
        Mplat_Game_Gnomograd        =  374, //  Гномоград
        Mplat_Game_Legendi_Drevnix  =  375, //  Легенды Древних
        Mplat_Game_LovePlanet       =  376, //  LovePlanet  Онлайн-сервисы и развлечения
        Mplat_Game_Videochat_Beseda =  377, //  Видеочат Беседа  Онлайн-сервисы и развлечения

        /**********************/
        
        Munis_HOT_WATER  = 215 ,
        Munis_Gaz        = 216,
        Munis_COLD_WATER = 217,
        Munis_Gos_Tamoj_Komitet = 365,
        
        Infin_Kredit  = 170,
        
        
        
        MM_LLC_TEST_MERCHANT = 379,
//        Munis_HOT_WATER      = 1530000 // NOT INSERTED YET!!!
        /////////////////////////////////////////////////
        Kafolat_insurance = 380,


        /********************************/
        Webmoney_Direct = 384,
        
        
        /************************/
        AsiaTV = 399,
        /*************************/
        Elmakon_Direct = 414,
        /**************************/
        Insurance_TEZ_Leasing	= 421,
        Insurance_Avant_Avto	= 422,
        
        /************************/
        Qiwi_Wallet_Direct_RUB = 425,
        Qiwi_Wallet_Direct_USD = 433,
        Qiwi_Wallet_Direct_EUR = 434,
        
        /**** nativepay providers ****/
        Nativepay_test_merchant=  432,
        
        
        /************************/
        Uzmobile_GSM_new_api  = 508,
        Uzmobile_CDMA_new_api = 509,
        
        
        
        /*************************/
//        HermesGrantTest = 511,
        
    };
    

    static inline bool is_munis(int32_t id){
        return id == Munis_GUBDD       || 
               id == Munis_HOT_WATER   || 
               id == Munis_Gaz         || 
               id == Munis_COLD_WATER  || 
               id == Munis_Gos_Tamoj_Komitet || 
               id == Munis_KinderGarden ;
    }
    
    static inline bool is_nativepay(int32_t id ){
        return (id == Nativepay_test_merchant  ) ;
    }
    
    static inline bool is_money_mover( int32_t id) {
        return id == MM_LLC_TEST_MERCHANT ;
    }

    static inline bool is_texnomart( int32_t id ) { 
        return  id == Technomart_St_Almazar      ||
                id == Technomart_St_Amir_Temur   ||
                id == Technomart_St_Avliyo_Ota   ||
                id == Technomart_St_Beshariq     ||
                id == Technomart_St_Chilanzar    ||
                id == Technomart_St_Farxadskiy   ||
                id == Technomart_St_Kichik_Xalqa ||
                id == Technomart_St_Parkentskiy
                ;
    }
    
    static inline bool is_webmoney(int32_t id){
        return id == Webmoney_Direct;
    }
    
//    static inline bool is_qiwi( int32_t id ){ 
//        return id == Qiwi_Wallet_Direct_RUB || id == Qiwi_Wallet_Direct_USD || id == Qiwi_Wallet_Direct_EUR ; 
//    } 
};

struct Currency_info_T
{
    typedef int32_t integer;
    integer id;
    bool   initialized ;
    double usd_uzs     ; // 1 DOLLAR in sum 
    double usd_rub     ; // 1 RUBLE  in sum
    double usd_eur     ; // 1 EURO   in sum
    
    integer type       ; // 1 - Uzbekistan Center bank,  2-Russian Center Bank, ....
    //std::string upd_ts ;
    
    enum Tp{ Type_Uzb_CB = 1, Type_Rus_CB = 2 } ;
    
    Currency_info_T();
     
    double usd(double amount_uzs_tiyin ) const  ; 
    double rub( double amount_uzs_tiyin )const; 
    double eur(double amount_uzs_tiyin ) const; 
};


class Currencies_table
{
public:
    explicit Currencies_table(DB_T& db);
    ~Currencies_table();
    
    //get currency info where upd_ts >= NOW() - interval '1 day'.
    //@Note: for 'type'  see Currency_info_T::type  field.
    Currency_info_T get(int type)const;
    Currency_info_T last(int type)const;
    
    void update(const Currency_info_T& currency);
    int32_t add(const Currency_info_T& currency);
    
private:
    DB_T& m_db;
};

enum M_field_type_T 
{
    M_FIELD_TYPE_UNDEF    = 0,
    M_FIELD_TYPE_INPUT    = 1,
    M_FIELD_TYPE_LIST     = 2,
    M_FIELD_TYPE_LABEL    = 3,
    M_FIELD_TYPE_AMOUNT   = 4,
    M_FIELD_TYPE_CONTACTS = 5,
};
enum M_filial_flag_T
{
    M_FILIAL_UNDEF = 0,
    M_FILIAL_NO = 1,
    M_FILIAL_YES = 2,
};

struct Merch_group_info_T 
{
    typedef int32_t integer;
    //typedef int64_t bigint;
    typedef std::string text;
    
    integer id       ;
    text    name     ; // a general name ( rus )
    text    name_uzb ; // an uzbek name
    integer icon_id  ;
    integer position ;
    text    icon_path;

    Merch_group_info_T() {
        id = 0;
        icon_id = 0;
        position = 0;
    }
};

class Merchant_group_T
{
public:
    explicit Merchant_group_T(DB_T & db);
    ~Merchant_group_T();
    
    Error_T add(const Merch_group_info_T & data);
    Error_T edit(uint32_t id, const Merch_group_info_T &info);
    Error_T del(uint32_t id);
    Error_T list(uint32_t id, std::vector<Merch_group_info_T> &group_list);

private:
    DB_T &m_db;
};
/*******************************************************/

struct Merchant_info_T 
{
    typedef int32_t      integer ;
    typedef int64_t      bigint  ;
    typedef std::string  text    ;
    
    integer   id;
    text      name;
    integer   group;
    integer   status;
    bigint    min_amount;
    bigint    max_amount;

    ///////////////
    text      url;
    text      inn;
    text      contract;
    text      contract_date;
    text      mfo;
    text      checking_account;
    text      merchantId;
    text      terminalId;
    integer   port;
    integer   external;
    text      extern_service;
    integer   bank_id;
    ///////////////////

    integer   rate;
    bigint    rate_money;
    integer   position;
    
    integer  api_id; //from merchant_api  table  api_id.
    
    // Status
    // 0 => undef
    // 1 => HIDE, disabled.
    // 2 => SHOW
    // 3 => HIDE but enabled.
    
    text id_list;
    bigint commission(bigint amount)const;
    
    Error_T is_valid_amount(bigint amount)const;
    
    bool commission_subtracted()const;
    
    static std::string make_icon_link(int32_t id, const std::string & file_path);
    static std::string make_icon_web_path(int32_t id, const std::string& file_path ) ;
    
    Merchant_info_T() ;
};

struct Merchant_list_T {
    uint32_t count;
    std::vector<Merchant_info_T> list;

    Merchant_list_T() { count = 0; }
};

struct Merchant_field_T {
    enum{ USAGE_UNDEF = 0, USAGE_REQUEST = 1, USAGE_INFO = 2, USAGE_DETAIL_INFO = 3};
    typedef int32_t integer;
    typedef int64_t bigint;
    typedef std::string text;
    
    integer     merchant_id ;
    integer     fID         ;
    integer     parent_fID  ;
    text label       ;
    text label_uz1   ;
    text label_uz2   ;
    text prefix_label;
    integer    type        ;   // 0 => UNDEF, 1 => input, 2 => list, 3 => label , 4=> amount, 5=> contacts
    integer    input_digit ;
    integer    input_letter;
    integer    min_length  ;
    integer    max_length  ;
    integer    position    ;
    text param_name  ;
    integer    usage       ; // 0 => UNDEF, 1 => on request, 2 => on purchase-info, 3 => detail info.

    
    text value; //used on purchase_info.
    
    Merchant_field_T() {
        merchant_id = 0;
        fID = 0;
        parent_fID = 0;
        type = M_FIELD_TYPE_UNDEF;
        input_digit = 0;
        input_letter = 0;
        position = 0;
        min_length = 0;
        max_length = 0xFFFF;
        usage = USAGE_REQUEST;
    }
};
 
struct Merchant_field_data_T {
    typedef int32_t integer;
    typedef std::string text;
    
    integer id;
    integer fID;
    integer key;
    integer parent_key;
    text value;
    text prefix;
    integer extra_id;
    integer service_id;
    integer service_id_check;
    integer max_size;

    Merchant_field_data_T() {
        id = 0;
        fID = 0;
        key = 0;
        parent_key = 0;
        extra_id = 0;
        service_id = 0;
        service_id_check = 0;
        max_size = 0xFFFF;
    }
};

struct fid_comparator
{
    typedef int32_t integer;
    struct fid
    {
        
        integer value;

        //implicitly converted from int
        fid( integer v)
        : value( v )
        {}

        //implicityly converted from merchant_field_data.
        fid(const Merchant_field_data_T& fdata)
        : value(fdata.fID)
        {}
        bool operator()(integer value)const{ return this->value == value;}
        bool operator()(const Merchant_field_data_T& fdata)const{ return this->value == fdata.fID;}
    };
    
    bool operator()(fid lhs, fid rhs)const { return lhs.value < rhs.value; } ;
    struct equal{  
        int32_t  id; 
        //implicit convert
        equal( int32_t  id) : id(id){}
        bool operator()(fid other )const { return id == other.value; } 
    } ;
};



struct Merchant_bonus_info_T
{
    enum StatusE{ StatusE_none = 0, StatusE_enabled = 1, StatusE_disabled = 2};
    
    int64_t     id            ;
    int32_t     merchant_id   ;
    int64_t     min_amount    ;
    int64_t     bonus_amount  ;
    int32_t     percent       ;
    int32_t     group_id      ;
    std::string  start_date    ;
    std::string  end_date      ;
    std::string  description   ;
    int          status        ;  // StatusE values.
    std::string  longitude     ;
    std::string  latitude      ;
    
    bool active; // need for search.
    //fill with zero integer members.
    Merchant_bonus_info_T()
    : id(), merchant_id(), min_amount()
    , bonus_amount(), percent(), group_id(), status()
    , longitude()
    , latitude()
    , active()
    {}
};
struct Merchant_bonus_list_T
{
    size_t total_count;
    std::vector< Merchant_bonus_info_T> list;
};



struct Merch_acc_T 
{
    int32_t id, api_id, status   ;
    std::string name;
    std::string login    ;
    std::string password ;
    std::string api_json ;
    std::string options  ;
    std::string url      ;
    
    inline Merch_acc_T(): id(0), api_id(0), status(0){}
};

struct Merchant_field_data_list_T
{
    int count;
    std::vector< Merchant_field_data_T> list;
};

struct Merchant_field_list_T
{
    int count;
    std::vector< Merchant_field_T> list;
};

class Merchant_T
{
public:
    Merchant_T(DB_T &db);

    
    Merchant_info_T get( int32_t id, Error_T& ec);
    
    Error_T add(const Merchant_info_T &m_info);
    Error_T edit(const Merchant_info_T &m_info);
    Error_T del(uint32_t id);
    
    Error_T info(const Merchant_info_T &search, Merchant_info_T &  data);
    Error_T list(const Merchant_info_T &search, const Sort_T &sort, Merchant_list_T & out_list);

    Error_T top_list(uint64_t uid, const Sort_T& sort, Merchant_list_T& out_list);
    
    Error_T acc_info(uint32_t merchant_id, Merch_acc_T& acc);
    
    Error_T api_info(int32_t api_id, Merch_acc_T& acc);
    
    std::vector< Merchant_field_T> request_fields( int32_t mid ) ;
    
    Error_T fields(const Merchant_field_T &search, const Sort_T& sort, Merchant_field_list_T&fields);
    Error_T field_info(uint32_t field_id, Merchant_field_T & data);
    Error_T field_add(const Merchant_field_T &data);
    Error_T field_edit(uint32_t field_id, const Merchant_field_T &data);
    Error_T field_delete(uint32_t field_id);

    Error_T field_data_list(const Merchant_field_data_T& search, const Sort_T& sort,  Merchant_field_data_list_T & f_dlist);
    Error_T field_data_search(const Merchant_field_data_T& search_param, const Sort_T & sort, Merchant_field_data_T& f_data);
    Error_T field_data_add(const Merchant_field_data_T& f_data);
    Error_T filed_data_edit(const Merchant_field_data_T& f_data);
    Error_T field_data_delete(const uint32_t id);

    Error_T qr_image(uint32_t merchant_id, std::string& data);
    Error_T generate_qr_image(uint32_t merchant_id);
    
    int64_t next_transaction_id();

    
    
    Error_T bonus_add(const Merchant_bonus_info_T& info, /*OUT*/  int64_t & id);
    Error_T bonus_list(const Merchant_bonus_info_T& search,  const Sort_T& sort, Merchant_bonus_list_T& list); 
    Error_T bonus_edit(uint64_t id, const Merchant_bonus_info_T& info);
    Error_T bonus_delete(uint64_t id);
private:
    DB_T &m_db;
};

#endif
