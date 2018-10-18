using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class purchases_reverseDataManager
    {
        //purchases_reverseViewModel
//publiclongid{get;set;}
//publiclong?pid{get;set;}
//publiclong?aid{get;set;}
//publiclong?uid{get;set;}
//publicstringsms_code{get;set;}
//publicint?status{get;set;}
//publicDateTime?ts_start{get;set;}
//publicDateTime?ts_confirm{get;set;}
//publicstringphone{get;set;}
//publicstringbaz{get;set;}

        public static void Add(purchases_reverseViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new purchases_reverse
            {
                id         = model.id           ,     
                pid        = model.pid          ,
                aid        = model.aid          ,
                uid        = model.uid          ,
                sms_code   = model.sms_code     ,
                status     = model.status       ,
                ts_start   = model.ts_start     ,
                ts_confirm = model.ts_confirm   ,
                phone      = model.phone        ,
                baz        = model.baz          ,
            };

            db.purchases_reverse.Add(dbmodel);
        }

        public static void Modify(purchases_reverseViewModel model, RAD_PAYEntities db)
        {
            var result = db.purchases_reverse.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id           ;     
                    dbmodel.pid = model.pid          ;
                    dbmodel.aid = model.aid          ;
                    dbmodel.uid = model.uid          ;
                    dbmodel.sms_code = model.sms_code     ;
                    dbmodel.status = model.status       ;
                    dbmodel.ts_start = model.ts_start     ;
                    dbmodel.ts_confirm = model.ts_confirm   ;
                    dbmodel.phone = model.phone        ;
                    dbmodel.baz = model.baz          ;
                }
            }
        }

        public static void Delete(purchases_reverseViewModel model, RAD_PAYEntities db)
        {
            var result = db.purchases_reverse.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.purchases_reverse.Remove(dbmodel);
                }
            }
        }

        public static List<purchases_reverseViewModel> Get(purchases_reverseViewModel model, RAD_PAYEntities db)
        {
            List<purchases_reverseViewModel> list = null;

            var query = from resmodel in db.purchases_reverse
                        select new purchases_reverseViewModel
                        {
                            id = resmodel.id,
                            pid = resmodel.pid,
                            aid = resmodel.aid,
                            uid = resmodel.uid,
                            sms_code = resmodel.sms_code,
                            status = resmodel.status,
                            ts_start = resmodel.ts_start,
                            ts_confirm = resmodel.ts_confirm,
                            phone = resmodel.phone,
                            baz = resmodel.baz,
                        };

            list = query.ToList();

            return list;
        }
    }
}