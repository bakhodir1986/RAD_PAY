using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class sms_infoDataManager
    {
        //sms_infoViewModel
//publiclongid{get;set;}
//publicstringmsg{get;set;}
//publicDateTime?ts{get;set;}
//publicint?send{get;set;}
//publicint?nphones{get;set;}
//publicstringphone{get;set;}
//publicint?type{get;set;}

        public static void Add(sms_infoViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new sms_info
            {
                id      = model.id      ,  
                msg     = model.msg     ,
                ts      = model.ts      ,
                send    = model.send    ,
                nphones = model.nphones ,
                phone   = model.phone   ,
                type    = model.type    ,
            };

            db.sms_info.Add(dbmodel);
        }

        public static void Modify(sms_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.sms_info.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id      ;  
                    dbmodel.msg = model.msg     ;
                    dbmodel.ts = model.ts      ;
                    dbmodel.send = model.send    ;
                    dbmodel.nphones = model.nphones ;
                    dbmodel.phone = model.phone   ;
                    dbmodel.type = model.type    ;
                }
            }
        }

        public static void Delete(sms_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.sms_info.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.sms_info.Remove(dbmodel);
                }
            }
        }

        public static List<sms_infoViewModel> Get(sms_infoViewModel model, RAD_PAYEntities db)
        {
            List<sms_infoViewModel> list = null;

            var query = from resmodel in db.sms_info
                        select new sms_infoViewModel
                        {
                            id = resmodel.id,
                            msg = resmodel.msg,
                            ts = resmodel.ts,
                            send = resmodel.send,
                            nphones = resmodel.nphones,
                            phone = resmodel.phone,
                            type = resmodel.type,
                        };

            list = query.ToList();

            return list;
        }
    }
}