using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchantDataManager
    {
        //merchantViewModel
//publicintid{get;set;}
//publicstringname{get;set;}
//publicstringurl{get;set;}
//publicint?group_id{get;set;}
//publicint?status{get;set;}
//publicstringinn{get;set;}
//publicstringcontract{get;set;}
//publicDateTime?contract_date{get;set;}
//publicstringmfo{get;set;}
//publicstringch_account{get;set;}
//publicstringmerchant_id{get;set;}
//publicstringterminal_id{get;set;}
//publicint?port{get;set;}
//publiclong?min_amount{get;set;}
//publiclong?max_amount{get;set;}
//publicintexternal{get;set;}
//publicstringext_service_id{get;set;}
//publicint?bank_id{get;set;}
//publicintrate{get;set;}
//publiclongrate_money{get;set;}
//publicint?position{get;set;}
//publicintapi_id{get;set;                }
        //public long icon_id { get; set; }

        public static void Add(merchantViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant
            {
                    id              = model.id              ,
                    name            = model.name            ,
                    url             = model.url             ,
                    group_id        = model.group_id        ,
                    status          = model.status          ,
                    inn             = model.inn             ,
                    contract        = model.contract        ,
                    contract_date   = model.contract_date   ,
                    mfo             = model.mfo             ,
                    ch_account      = model.ch_account      ,
                    merchant_id     = model.merchant_id     ,
                    terminal_id     = model.terminal_id     ,
                    port            = model.port            ,
                    min_amount      = model.min_amount      ,
                    max_amount      = model.max_amount      ,
                    external        = model.external        ,
                    ext_service_id  = model.ext_service_id  ,
                    bank_id         = model.bank_id         ,
                    rate            = model.rate            ,
                    rate_money      = model.rate_money      ,
                    position        = model.position        ,
                    api_id          = model.api_id          ,
            };

            db.merchants.Add(dbmodel);
        }

        public static void Modify(merchantViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchants.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id              ;
                    dbmodel.name = model.name            ;
                    dbmodel.url = model.url             ;
                    dbmodel.group_id = model.group_id        ;
                    dbmodel.status = model.status          ;
                    dbmodel.inn = model.inn             ;
                    dbmodel.contract = model.contract        ;
                    dbmodel.contract_date = model.contract_date   ;
                    dbmodel.mfo = model.mfo             ;
                    dbmodel.ch_account = model.ch_account      ;
                    dbmodel.merchant_id = model.merchant_id     ;
                    dbmodel.terminal_id = model.terminal_id     ;
                    dbmodel.port = model.port            ;
                    dbmodel.min_amount = model.min_amount      ;
                    dbmodel.max_amount = model.max_amount      ;
                    dbmodel.external = model.external        ;
                    dbmodel.ext_service_id = model.ext_service_id  ;
                    dbmodel.bank_id = model.bank_id         ;
                    dbmodel.rate = model.rate            ;
                    dbmodel.rate_money = model.rate_money      ;
                    dbmodel.position = model.position        ;
                    dbmodel.api_id = model.api_id          ;
                }
            }

        }

        public static void Delete(merchantViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchants.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchants.Remove(dbmodel);
                }
            }
        }

        public static List<merchantViewModel> Get(merchantViewModel model, RAD_PAYEntities db)
        {
            List<merchantViewModel> list = null;

            var query = from resmodel in db.merchants
                        select new merchantViewModel
                        {
                            id = resmodel.id,
                            name = resmodel.name,
                            url = resmodel.url,
                            group_id = resmodel.group_id,
                            status = resmodel.status,
                            inn = resmodel.inn,
                            contract = resmodel.contract,
                            contract_date = resmodel.contract_date,
                            mfo = resmodel.mfo,
                            ch_account = resmodel.ch_account,
                            merchant_id = resmodel.merchant_id,
                            terminal_id = resmodel.terminal_id,
                            port = resmodel.port,
                            min_amount = resmodel.min_amount,
                            max_amount = resmodel.max_amount,
                            external = resmodel.external,
                            ext_service_id = resmodel.ext_service_id,
                            bank_id = resmodel.bank_id,
                            rate = resmodel.rate,
                            rate_money = resmodel.rate_money,
                            position = resmodel.position,
                            api_id = resmodel.api_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}