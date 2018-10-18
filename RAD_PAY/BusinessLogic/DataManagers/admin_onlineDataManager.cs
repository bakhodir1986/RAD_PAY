using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class admin_onlineDataManager
    {
        //admin_onlineViewModel
        //public long       aid         { get; set; }
        //public string     token       { get; set; }
        //public DateTime?  login_ts    { get; set; }
        //public DateTime?  last_ts     { get; set; }

        public static void Add(admin_onlineViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new admin_online
            {
                aid        = model.aid      ,  
                token      = model.token    ,
                login_ts   = model.login_ts ,
                last_ts    = model.last_ts  
            };

            db.admin_online.Add(dbmodel);
        }

        public static void Modify(admin_onlineViewModel model, RAD_PAYEntities db)
        {
            var result = db.admin_online.Where(z => z.aid == model.aid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.aid = model.aid             ;  
                    dbmodel.token = model.token         ;
                    dbmodel.login_ts = model.login_ts   ;
                    dbmodel.last_ts = model.last_ts;
                }
            }
        }

        public static void Delete(admin_onlineViewModel model, RAD_PAYEntities db)
        {
            var result = db.admin_online.Where(z => z.aid == model.aid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.admin_online.Remove(dbmodel);
                }
            }
        }

        public static List<admin_onlineViewModel> Get(admin_onlineViewModel model, RAD_PAYEntities db)
        {
            List<admin_onlineViewModel> list = null;

            var query = from resmodel in db.admin_online
                        select new admin_onlineViewModel
                        {
                            aid        = resmodel.aid       ,
                            token      = resmodel.token     ,
                            login_ts   = resmodel.login_ts  ,
                            last_ts    = resmodel.last_ts   ,
                        };

            list = query.ToList();

            return list;
        }
    }
}