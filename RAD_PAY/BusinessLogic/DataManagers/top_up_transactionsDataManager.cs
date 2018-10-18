using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class top_up_transactionsDataManager
    {
        //top_up_transactionsViewModel
//publiclongid{get;set;}
//publicint?topup_id{get;set;}
//publiclong?amount_sum{get;set;}
//publicdouble?amount_req{get;set;}
//publicint?currency{get;set;}
//publiclong?uid{get;set;}
//publicstringlogin{get;set;}
//publicDateTime?ts{get;set;}
//publicDateTime?tse{get;set;}
//publicint?status{get;set;}
//publicstringstatus_text{get;set;}
//publiclong?card_id{get;set;}
//publicstringcard_pan{get;set;}
//publicstringeopc_trn_id{get;set;}
//publiclong?rad_card{get;set;}
//publicstringpay_description{get;set;}
//publiclong?topup_trn_id{get;set;}

        public static void Add(top_up_transactionsViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new top_up_transactions
            {
                id              = model.id              ,     
                topup_id        = model.topup_id        ,
                amount_sum      = model.amount_sum      ,
                amount_req      = model.amount_req      ,
                currency        = model.currency        ,
                uid             = model.uid             ,
                login           = model.login           ,
                ts              = model.ts              ,
                tse             = model.tse             ,
                status          = model.status          ,
                status_text     = model.status_text     ,
                card_id         = model.card_id         ,
                card_pan        = model.card_pan        ,
                eopc_trn_id     = model.eopc_trn_id     ,
                rad_card        = model.rad_card        ,
                pay_description = model.pay_description ,
                topup_trn_id    = model.topup_trn_id    ,
            };

            db.top_up_transactions.Add(dbmodel);
        }

        public static void Modify(top_up_transactionsViewModel model, RAD_PAYEntities db)
        {
            var result = db.top_up_transactions.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id              ;     
                    dbmodel.topup_id = model.topup_id        ;
                    dbmodel.amount_sum = model.amount_sum      ;
                    dbmodel.amount_req = model.amount_req      ;
                    dbmodel.currency = model.currency        ;
                    dbmodel.uid = model.uid             ;
                    dbmodel.login = model.login           ;
                    dbmodel.ts = model.ts              ;
                    dbmodel.tse = model.tse             ;
                    dbmodel.status = model.status          ;
                    dbmodel.status_text = model.status_text     ;
                    dbmodel.card_id = model.card_id         ;
                    dbmodel.card_pan = model.card_pan        ;
                    dbmodel.eopc_trn_id = model.eopc_trn_id     ;
                    dbmodel.rad_card = model.rad_card        ;
                    dbmodel.pay_description = model.pay_description ;
                    dbmodel.topup_trn_id = model.topup_trn_id    ;
                }
            }
        }

        public static void Delete(top_up_transactionsViewModel model, RAD_PAYEntities db)
        {
            var result = db.top_up_transactions.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.top_up_transactions.Remove(dbmodel);
                }
            }
        }

        public static List<top_up_transactionsViewModel> Get(top_up_transactionsViewModel model, RAD_PAYEntities db)
        {
            List<top_up_transactionsViewModel> list = null;

            var query = from resmodel in db.top_up_transactions
                        select new top_up_transactionsViewModel
                        {
                            id = resmodel.id,
                            topup_id = resmodel.topup_id,
                            amount_sum = resmodel.amount_sum,
                            amount_req = resmodel.amount_req,
                            currency = resmodel.currency,
                            uid = resmodel.uid,
                            login = resmodel.login,
                            ts = resmodel.ts,
                            tse = resmodel.tse,
                            status = resmodel.status,
                            status_text = resmodel.status_text,
                            card_id = resmodel.card_id,
                            card_pan = resmodel.card_pan,
                            eopc_trn_id = resmodel.eopc_trn_id,
                            rad_card = resmodel.rad_card,
                            pay_description = resmodel.pay_description,
                            topup_trn_id = resmodel.topup_trn_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}