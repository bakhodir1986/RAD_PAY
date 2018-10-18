using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class transactionDataManager
    {
        //transactionViewModel
//publiclongid{get;set;}
//publiclong?uid{get;set;}
//publicstringsrccard{get;set;}
//publicstringdstcard{get;set;}
//publicstringsrcphone{get;set;}
//publicstringdstphone{get;set;}
//publiclong?amount{get;set;}
//publicDateTime?ts{get;set;}
//publicint?status{get;set;}
//publiclong?dst_uid{get;set;}
//publiclong?commision{get;set;}
//publicstringref_num{get;set;}
//publiclong?bearn{get;set;}
//publicstringstatus_text{get;set;}

        public static void Add(transactionViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new transaction
            {
                id          = model.id          , 
                uid         = model.uid         ,
                srccard     = model.srccard     ,
                dstcard     = model.dstcard     ,
                srcphone    = model.srcphone    ,
                dstphone    = model.dstphone    ,
                amount      = model.amount      ,
                ts          = model.ts          ,
                status      = model.status      ,
                dst_uid     = model.dst_uid     ,
                commision   = model.commision   ,
                ref_num     = model.ref_num     ,
                bearn       = model.bearn       ,
                status_text = model.status_text ,
            };

            db.transactions.Add(dbmodel);
        }

        public static void Modify(transactionViewModel model, RAD_PAYEntities db)
        {
            var result = db.transactions.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id          ; 
                    dbmodel.uid = model.uid         ;
                    dbmodel.srccard = model.srccard     ;
                    dbmodel.dstcard = model.dstcard     ;
                    dbmodel.srcphone = model.srcphone    ;
                    dbmodel.dstphone = model.dstphone    ;
                    dbmodel.amount = model.amount      ;
                    dbmodel.ts = model.ts          ;
                    dbmodel.status = model.status      ;
                    dbmodel.dst_uid = model.dst_uid     ;
                    dbmodel.commision = model.commision   ;
                    dbmodel.ref_num = model.ref_num     ;
                    dbmodel.bearn = model.bearn       ;
                    dbmodel.status_text = model.status_text ;
                }
            }
        }

        public static void Delete(transactionViewModel model, RAD_PAYEntities db)
        {
            var result = db.transactions.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.transactions.Remove(dbmodel);
                }
            }
        }

        public static List<transactionViewModel> Get(transactionViewModel model, RAD_PAYEntities db)
        {
            List<transactionViewModel> list = null;

            var query = from resmodel in db.transactions
                        select new transactionViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            srccard = resmodel.srccard,
                            dstcard = resmodel.dstcard,
                            srcphone = resmodel.srcphone,
                            dstphone = resmodel.dstphone,
                            amount = resmodel.amount,
                            ts = resmodel.ts,
                            status = resmodel.status,
                            dst_uid = resmodel.dst_uid,
                            commision = resmodel.commision,
                            ref_num = resmodel.ref_num,
                            bearn = resmodel.bearn,
                            status_text = resmodel.status_text,
                        };

            list = query.ToList();

            return list;
        }
    }
}