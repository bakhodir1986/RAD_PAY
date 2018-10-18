using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class purchaseDataManager
    {
        //purchaseViewModel
//publiclongid{get;set;}
//publicintmerchant_id{get;set;}
//publicstringlogin{get;set;}
//publiclong?amount{get;set;}
//publicDateTime?ts{get;set;}
//publiclong?uid{get;set;}
//publicstringtransaction_id{get;set;}
//publicstringpan{get;set;}
//publicint?status{get;set;}
//publicstringpaynet_tr_id{get;set;}
//publicintpaynet_status{get;set;}
//publiclong?receipt{get;set;}
//publiclongrad_paynet_tr_id{get;set;}
//publiclong?rad_tr_id{get;set;}
//publiclong?card_id{get;set;}
//publiclong?bearn{get;set;}
//publiclongcommission{get;set;}
//publicstringmerch_rsp{get;set;}


        public static void Add(purchaseViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new purchase
            {
                id                  = model.id               ,        
                merchant_id         = model.merchant_id      ,
                login               = model.login            ,
                amount              = model.amount           ,
                ts                  = model.ts               ,
                uid                 = model.uid              ,
                transaction_id      = model.transaction_id   ,
                pan                 = model.pan              ,
                status              = model.status           ,
                paynet_tr_id        = model.paynet_tr_id     ,
                paynet_status       = model.paynet_status    ,
                receipt             = model.receipt          ,
                rad_paynet_tr_id    = model.rad_paynet_tr_id ,
                rad_tr_id           = model.rad_tr_id        ,
                card_id             = model.card_id          ,
                bearn               = model.bearn            ,
                commission          = model.commission       ,
                merch_rsp           = model.merch_rsp        ,
            };

            db.purchases.Add(dbmodel);
        }

        public static void Modify(purchaseViewModel model, RAD_PAYEntities db)
        {
            var result = db.purchases.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id               ;        
                    dbmodel.merchant_id = model.merchant_id      ;
                    dbmodel.login = model.login            ;
                    dbmodel.amount = model.amount           ;
                    dbmodel.ts = model.ts               ;
                    dbmodel.uid = model.uid              ;
                    dbmodel.transaction_id = model.transaction_id   ;
                    dbmodel.pan = model.pan              ;
                    dbmodel.status = model.status           ;
                    dbmodel.paynet_tr_id = model.paynet_tr_id     ;
                    dbmodel.paynet_status = model.paynet_status    ;
                    dbmodel.receipt = model.receipt          ;
                    dbmodel.rad_paynet_tr_id = model.rad_paynet_tr_id ;
                    dbmodel.rad_tr_id = model.rad_tr_id        ;
                    dbmodel.card_id = model.card_id          ;
                    dbmodel.bearn = model.bearn            ;
                    dbmodel.commission = model.commission       ;
                    dbmodel.merch_rsp = model.merch_rsp        ;
                }
            }
        }

        public static void Delete(purchaseViewModel model, RAD_PAYEntities db)
        {
            var result = db.purchases.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.purchases.Remove(dbmodel);
                }
            }
        }

        public static List<purchaseViewModel> Get(purchaseViewModel model, RAD_PAYEntities db)
        {
            List<purchaseViewModel> list = null;

            var query = from resmodel in db.purchases
                        select new purchaseViewModel
                        {
                            id = resmodel.id,
                            merchant_id = resmodel.merchant_id,
                            login = resmodel.login,
                            amount = resmodel.amount,
                            ts = resmodel.ts,
                            uid = resmodel.uid,
                            transaction_id = resmodel.transaction_id,
                            pan = resmodel.pan,
                            status = resmodel.status,
                            paynet_tr_id = resmodel.paynet_tr_id,
                            paynet_status = resmodel.paynet_status,
                            receipt = resmodel.receipt,
                            rad_paynet_tr_id = resmodel.rad_paynet_tr_id,
                            rad_tr_id = resmodel.rad_tr_id,
                            card_id = resmodel.card_id,
                            bearn = resmodel.bearn,
                            commission = resmodel.commission,
                            merch_rsp = resmodel.merch_rsp,
                        };

            list = query.ToList();

            return list;
        }
    }
}