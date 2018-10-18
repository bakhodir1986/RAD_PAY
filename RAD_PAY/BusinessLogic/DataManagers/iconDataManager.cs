using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class iconDataManager
    {
        //iconViewModel
        //public long       id          { get; set; }
        //public string     location    { get; set; }
        //public string     path_hash   { get; set; }
        //public int?       kind        { get; set; }
        //public DateTime?  ts          { get; set; }
        //public long?      size        { get; set; }
        //public string     sha1_sum    { get; set; }
        //public int?       b           { get; set; }
        //public int?       c           { get; set; }
        //public int?       d           { get; set; }

        public static void Add(iconViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new icon
            {
                id          = model.id          , 
                location    = model.location    ,
                path_hash   = model.path_hash   ,
                kind        = model.kind        ,
                ts          = model.ts          ,
                size        = model.size        ,
                sha1_sum    = model.sha1_sum    ,
                b           = model.b           ,
                c           = model.c           ,
                d           = model.d           ,
            };

            db.icons.Add(dbmodel);
        }

        public static void Modify(iconViewModel model, RAD_PAYEntities db)
        {
            var result = db.icons.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                   ; 
                    dbmodel.location = model.location       ;
                    dbmodel.path_hash = model.path_hash     ;
                    dbmodel.kind = model.kind               ;
                    dbmodel.ts = model.ts                   ;
                    dbmodel.size = model.size               ;
                    dbmodel.sha1_sum = model.sha1_sum       ;
                    dbmodel.b = model.b                     ;
                    dbmodel.c = model.c                     ;
                    dbmodel.d = model.d;
                }
            }
        }

        public static void Delete(iconViewModel model, RAD_PAYEntities db)
        {
            var result = db.icons.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.icons.Remove(dbmodel);
                }
            }
        }

        public static List<iconViewModel> Get(iconViewModel model, RAD_PAYEntities db)
        {
            List<iconViewModel> list = null;

            var query = from resmodel in db.icons
                        select new iconViewModel
                        {
                            id = resmodel.id,
                            location = resmodel.location,
                            path_hash = resmodel.path_hash,
                            kind = resmodel.kind,
                            ts = resmodel.ts,
                            size = resmodel.size,
                            sha1_sum = resmodel.sha1_sum,
                            b = resmodel.b,
                            c = resmodel.c,
                            d = resmodel.d,
                        };

            list = query.ToList();

            return list;
        }
    }
}