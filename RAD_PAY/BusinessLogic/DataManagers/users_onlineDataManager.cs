using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class users_onlineDataManager
    {
        //users_onlineViewModel
//publiclonguid{get;set;}
//publicstringtoken{get;set;}
//publicDateTimelogin_time{get;set;}
//publicDateTimelast_online{get;set;}
//publiclong?dev_id{get;set;}

        public static void Add(users_onlineViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new users_online
            {
                uid             = model.uid         ,  
                token           = model.token       ,
                login_time      = model.login_time  ,
                last_online     = model.last_online ,
                dev_id          = model.dev_id      ,
            };

            db.users_online.Add(dbmodel);
        }

        public static void Modify(users_onlineViewModel model, RAD_PAYEntities db)
        {
            var result = db.users_online.Where(z => z.uid == model.uid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.uid = model.uid         ;  
                    dbmodel.token = model.token       ;
                    dbmodel.login_time = model.login_time  ;
                    dbmodel.last_online = model.last_online ;
                    dbmodel.dev_id = model.dev_id      ;
                }
            }
        }

        public static void Delete(users_onlineViewModel model, RAD_PAYEntities db)
        {
            var result = db.users_online.Where(z => z.uid == model.uid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.users_online.Remove(dbmodel);
                }
            }
        }

        public static List<users_onlineViewModel> Get(users_onlineViewModel model, RAD_PAYEntities db)
        {
            List<users_onlineViewModel> list = null;

            var query = from resmodel in db.users_online
                        select new users_onlineViewModel
                        {
                            uid = resmodel.uid,
                            token = resmodel.token,
                            login_time = resmodel.login_time,
                            last_online = resmodel.last_online,
                            dev_id = resmodel.dev_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}