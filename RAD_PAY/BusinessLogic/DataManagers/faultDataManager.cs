using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class faultDataManager
    {
        //faultViewModel
        //public long       id          { get; set; }
        //public int?       type        { get; set; }
        //public DateTime?  ts          { get; set; }
        //public int?       status      { get; set; }
        //public string     description { get; set; }
        //public DateTime?  ts_notify   { get; set; }

        public static void Add(faultViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new fault
            {
                id          = model.id          , 
                type        = model.type        ,
                ts          = model.ts          ,
                status      = model.status      ,
                description = model.description ,
                ts_notify   = model.ts_notify   ,
            };

            db.faults.Add(dbmodel);
        }

        public static void Modify(faultViewModel model, RAD_PAYEntities db)
        {
            var result = db.faults.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                   ; 
                    dbmodel.type = model.type               ;
                    dbmodel.ts = model.ts                   ;
                    dbmodel.status = model.status           ;
                    dbmodel.description = model.description ;
                    dbmodel.ts_notify = model.ts_notify;
                }
            }
        }

        public static void Delete(faultViewModel model, RAD_PAYEntities db)
        {
            var result = db.faults.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.faults.Remove(dbmodel);
                }
            }
        }

        public static List<faultViewModel> Get(faultViewModel model, RAD_PAYEntities db)
        {
            List<faultViewModel> list = null;

            var query = from resmodel in db.faults
                        select new faultViewModel
                        {
                            id = resmodel.id,
                            type = resmodel.type,
                            ts = resmodel.ts,
                            status = resmodel.status,
                            description = resmodel.description,
                            ts_notify = resmodel.ts_notify,
                        };

            list = query.ToList();

            return list;
        }
    }
}