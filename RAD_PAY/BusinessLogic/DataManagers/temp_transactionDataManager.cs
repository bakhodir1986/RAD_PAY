using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class temp_transactionDataManager
    {
        //temp_transactionViewModel
//publiclongid{get;set;}
//publiclong?uid{get;set;}
//publiclong?dst_uid{get;set;}
//publiclongamount{get;set;}
//publicDateTimecreate_ts{get;set;}
//publicstringtoken{get;set;}
//publicstringdstcard{get;set;}

        public static void Add(temp_transactionViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new temp_transaction
            {
                id          = model.id          ,
                uid         = model.uid         ,
                dst_uid     = model.dst_uid     ,
                amount      = model.amount      ,
                create_ts   = model.create_ts   ,
                token       = model.token       ,
                dstcard     = model.dstcard     ,
            };

            db.temp_transaction.Add(dbmodel);
        }

        public static void Modify(temp_transactionViewModel model, RAD_PAYEntities db)
        {
            var result = db.temp_transaction.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id          ;
                    dbmodel.uid = model.uid         ;
                    dbmodel.dst_uid = model.dst_uid     ;
                    dbmodel.amount = model.amount      ;
                    dbmodel.create_ts = model.create_ts   ;
                    dbmodel.token = model.token       ;
                    dbmodel.dstcard = model.dstcard     ;
                }
            }
        }

        public static void Delete(temp_transactionViewModel model, RAD_PAYEntities db)
        {
            var result = db.temp_transaction.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.temp_transaction.Remove(dbmodel);
                }
            }
        }

        public static List<temp_transactionViewModel> Get(temp_transactionViewModel model, RAD_PAYEntities db)
        {
            List<temp_transactionViewModel> list = null;

            var query = from resmodel in db.temp_transaction
                        select new temp_transactionViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            dst_uid = resmodel.dst_uid,
                            amount = resmodel.amount,
                            create_ts = resmodel.create_ts,
                            token = resmodel.token,
                            dstcard = resmodel.dstcard,
                        };

            list = query.ToList();

            return list;
        }
    }
}