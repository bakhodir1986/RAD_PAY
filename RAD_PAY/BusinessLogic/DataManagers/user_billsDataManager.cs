using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class user_billsDataManager
    {
//user_billsViewModel
//publiclongid{get;set;}
//publiclong?uid{get;set;}
//publiclong?uid2{get;set;}
//publiclong?amount{get;set;}
//publicint?merchant_id{get;set;}
//publicstringvalue{get;set;}
//publicint?status{get;set;}
//publicDateTime?add_ts{get;set;}
//publicstringcomment{get;set;}

        public static void Add(user_billsViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new user_bills
            {
                id              = model.id          ,
                uid             = model.uid         ,
                uid2            = model.uid2        ,
                amount          = model.amount      ,
                merchant_id     = model.merchant_id ,
                value           = model.value       ,
                status          = model.status      ,
                add_ts          = model.add_ts      ,
                comment         = model.comment     ,
            };

            db.user_bills.Add(dbmodel);
        }

        public static void Modify(user_billsViewModel model, RAD_PAYEntities db)
        {
            var result = db.user_bills.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id          ;
                    dbmodel.uid = model.uid         ;
                    dbmodel.uid2 = model.uid2        ;
                    dbmodel.amount = model.amount      ;
                    dbmodel.merchant_id = model.merchant_id ;
                    dbmodel.value = model.value       ;
                    dbmodel.status = model.status      ;
                    dbmodel.add_ts = model.add_ts      ;
                    dbmodel.comment = model.comment     ;
                }
            }
        }

        public static void Delete(user_billsViewModel model, RAD_PAYEntities db)
        {
            var result = db.user_bills.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.user_bills.Remove(dbmodel);
                }
            }
        }

        public static List<user_billsViewModel> Get(user_billsViewModel model, RAD_PAYEntities db)
        {
            List<user_billsViewModel> list = null;

            var query = from resmodel in db.user_bills
                        select new user_billsViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            uid2 = resmodel.uid2,
                            amount = resmodel.amount,
                            merchant_id = resmodel.merchant_id,
                            value = resmodel.value,
                            status = resmodel.status,
                            add_ts = resmodel.add_ts,
                            comment = resmodel.comment,
                        };

            list = query.ToList();

            return list;
        }
    }
}