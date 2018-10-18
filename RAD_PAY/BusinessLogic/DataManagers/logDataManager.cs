using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class logDataManager
    {
        //logViewModel
        //public long           id          { get; set; }
        //public int?           aid         { get; set; }
        //public DateTime?      add_ts      { get; set; }
        //public string         action      { get; set; }

        public static void Add(logViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new log
            {
                id      = model.id      ,
                aid     = model.aid     ,
                add_ts  = model.add_ts  ,
                action  = model.action  ,
            };

            db.logs.Add(dbmodel);
        }

        public static void Modify(logViewModel model, RAD_PAYEntities db)
        {
            var result = db.logs.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id          ;
                    dbmodel.aid = model.aid        ;
                    dbmodel.add_ts = model.add_ts  ;
                    dbmodel.action = model.action;
                }
            }
        }

        public static void Delete(logViewModel model, RAD_PAYEntities db)
        {
            var result = db.logs.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.logs.Remove(dbmodel);
                }
            }
        }

        public static List<logViewModel> Get(logViewModel model, RAD_PAYEntities db)
        {
            List<logViewModel> list = null;

            var query = from resmodel in db.logs
                        select new logViewModel
                        {
                            id = resmodel.id,
                            aid = resmodel.aid,
                            add_ts = resmodel.add_ts,
                            action = resmodel.action,
                        };

            list = query.ToList();

            return list;
        }
    }
}