using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class periodic_billDataManager
    {
        //periodic_billViewModel
//publiclongid{get;set;}
//publiclong?uid{get;set;}
//publicint?merchant_id{get;set;}
//publicint?field_id{get;set;}
//publiclong?amount{get;set;}
//publicstringname{get;set;}
//publicstringvalue{get;set;}
//publicstringperiodic_ts{get;set;}
//publicstringprefix{get;set;}
//publicDateTimeadd_ts{get;set;}
//publicint?status{get;set;}
//publicDateTime?last_bill_ts{get;set;}
//publicDateTime?last_notify_ts{get;set;}
//publiclongcard_id{get;set;}

        public static void Add(periodic_billViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new periodic_bill
            {
                id             = model.id               ,        
                uid            = model.uid              ,
                merchant_id    = model.merchant_id      ,
                field_id       = model.field_id         ,
                amount         = model.amount           ,
                name           = model.name             ,
                value          = model.value            ,
                periodic_ts    = model.periodic_ts      ,
                prefix         = model.prefix           ,
                add_ts         = model.add_ts           ,
                status         = model.status           ,
                last_bill_ts   = model.last_bill_ts     ,
                last_notify_ts = model.last_notify_ts   ,
                card_id        = model.card_id          ,
            };

            db.periodic_bill.Add(dbmodel);
        }

        public static void Modify(periodic_billViewModel model, RAD_PAYEntities db)
        {
            var result = db.periodic_bill.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id               ;        
                    dbmodel.uid = model.uid              ;
                    dbmodel.merchant_id = model.merchant_id      ;
                    dbmodel.field_id = model.field_id         ;
                    dbmodel.amount = model.amount           ;
                    dbmodel.name = model.name             ;
                    dbmodel.value = model.value            ;
                    dbmodel.periodic_ts = model.periodic_ts      ;
                    dbmodel.prefix = model.prefix           ;
                    dbmodel.add_ts = model.add_ts           ;
                    dbmodel.status = model.status           ;
                    dbmodel.last_bill_ts = model.last_bill_ts     ;
                    dbmodel.last_notify_ts = model.last_notify_ts   ;
                    dbmodel.card_id = model.card_id          ;
                }
            }
        }

        public static void Delete(periodic_billViewModel model, RAD_PAYEntities db)
        {
            var result = db.periodic_bill.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.periodic_bill.Remove(dbmodel);
                }
            }
        }

        public static List<periodic_billViewModel> Get(periodic_billViewModel model, RAD_PAYEntities db)
        {
            List<periodic_billViewModel> list = null;

            var query = from resmodel in db.periodic_bill
                        select new periodic_billViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            merchant_id = resmodel.merchant_id,
                            field_id = resmodel.field_id,
                            amount = resmodel.amount,
                            name = resmodel.name,
                            value = resmodel.value,
                            periodic_ts = resmodel.periodic_ts,
                            prefix = resmodel.prefix,
                            add_ts = resmodel.add_ts,
                            status = resmodel.status,
                            last_bill_ts = resmodel.last_bill_ts,
                            last_notify_ts = resmodel.last_notify_ts,
                            card_id = resmodel.card_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}